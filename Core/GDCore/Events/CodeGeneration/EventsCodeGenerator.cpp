#include "GDCore/Events/CodeGeneration/EventsCodeGenerator.h"

#include <algorithm>
#include <utility>

#include "GDCore/CommonTools.h"
#include "GDCore/Events/CodeGeneration/EventsCodeGenerationContext.h"
#include "GDCore/Events/CodeGeneration/ExpressionCodeGenerator.h"
#include "GDCore/Events/Tools/EventsCodeNameMangler.h"
#include "GDCore/Extensions/Metadata/BehaviorMetadata.h"
#include "GDCore/Extensions/Metadata/InstructionMetadata.h"
#include "GDCore/Extensions/Metadata/MetadataProvider.h"
#include "GDCore/Extensions/Metadata/ObjectMetadata.h"
#include "GDCore/Extensions/Metadata/ParameterMetadataTools.h"
#include "GDCore/Extensions/Platform.h"
#include "GDCore/Extensions/PlatformExtension.h"
#include "GDCore/Project/Layout.h"
#include "GDCore/Project/ObjectsContainer.h"
#include "GDCore/Project/Project.h"

using namespace std;

namespace gd {

/**
 * Generate call using a relational operator.
 * Relational operator position is deduced from parameters type.
 * Rhs hand side expression is assumed to be placed just before the relational
 * operator.
 *
 * \param Information about the instruction
 * \param Arguments, in their C++ form.
 * \param String to be placed at the start of the call ( the function to be
 * called typically ). Example : MyObject->Get \param Arguments will be
 * generated starting from this number. For example, set this to 1 to skip the
 * first argument.
 */
gd::String EventsCodeGenerator::GenerateRelationalOperatorCall(
    const gd::InstructionMetadata& instrInfos,
    const vector<gd::String>& arguments,
    const gd::String& callStartString,
    std::size_t startFromArgument) {
  std::size_t relationalOperatorIndex = instrInfos.parameters.size();
  for (std::size_t i = startFromArgument; i < instrInfos.parameters.size();
       ++i) {
    if (instrInfos.parameters[i].type == "relationalOperator")
      relationalOperatorIndex = i;
  }
  // Ensure that there is at least one parameter after the relational operator
  if (relationalOperatorIndex + 1 >= instrInfos.parameters.size()) {
    ReportError();
    return "";
  }

  gd::String relationalOperator = arguments[relationalOperatorIndex];
  if (relationalOperator.size() > 2)
    relationalOperator = relationalOperator.substr(
        1,
        relationalOperator.length() - 1 -
            1);  // Relational operator contains quote which must be removed.

  gd::String rhs = arguments[relationalOperatorIndex + 1];
  gd::String argumentsStr;
  for (std::size_t i = startFromArgument; i < arguments.size(); ++i) {
    if (i != relationalOperatorIndex && i != relationalOperatorIndex + 1) {
      if (!argumentsStr.empty()) argumentsStr += ", ";
      argumentsStr += arguments[i];
    }
  }

  return callStartString + "(" + argumentsStr + ") " + relationalOperator +
         " " + rhs;
}

/**
 * Generate call using an operator ( =,+,-,*,/ ).
 * Operator position is deduced from parameters type.
 * Expression is assumed to be placed just before the operator.
 *
 * \param Information about the instruction
 * \param Arguments, in their C++ form.
 * \param String to be placed at the start of the call ( the function to be
 * called typically ). Example : MyObject->Set \param String to be placed at the
 * start of the call of the getter ( the "getter" function to be called
 * typically ). Example : MyObject->Get \param Arguments will be generated
 * starting from this number. For example, set this to 1 to skip the first
 * argument.
 */
gd::String EventsCodeGenerator::GenerateOperatorCall(
    const gd::InstructionMetadata& instrInfos,
    const vector<gd::String>& arguments,
    const gd::String& callStartString,
    const gd::String& getterStartString,
    std::size_t startFromArgument) {
  std::size_t operatorIndex = instrInfos.parameters.size();
  for (std::size_t i = startFromArgument; i < instrInfos.parameters.size();
       ++i) {
    if (instrInfos.parameters[i].type == "operator") operatorIndex = i;
  }

  // Ensure that there is at least one parameter after the operator
  if (operatorIndex + 1 >= instrInfos.parameters.size()) {
    ReportError();
    return "";
  }

  gd::String operatorStr = arguments[operatorIndex];
  if (operatorStr.size() > 2)
    operatorStr = operatorStr.substr(
        1,
        operatorStr.length() - 1 -
            1);  // Operator contains quote which must be removed.

  gd::String rhs = arguments[operatorIndex + 1];

  // Generate arguments for calling the "getter" function
  gd::String getterArgumentsStr;
  for (std::size_t i = startFromArgument; i < arguments.size(); ++i) {
    if (i != operatorIndex && i != operatorIndex + 1) {
      if (!getterArgumentsStr.empty()) getterArgumentsStr += ", ";
      getterArgumentsStr += arguments[i];
    }
  }

  // Generate arguments for calling the function ("setter")
  gd::String argumentsStr;
  for (std::size_t i = startFromArgument; i < arguments.size(); ++i) {
    if (i != operatorIndex &&
        i != operatorIndex + 1)  // Generate classic arguments
    {
      if (!argumentsStr.empty()) argumentsStr += ", ";
      argumentsStr += arguments[i];
    }
    if (i == operatorIndex + 1) {
      if (!argumentsStr.empty()) argumentsStr += ", ";
      if (operatorStr != "=")
        argumentsStr += getterStartString + "(" + getterArgumentsStr + ") " +
                        operatorStr + " (" + rhs + ")";
      else
        argumentsStr += rhs;
    }
  }

  return callStartString + "(" + argumentsStr + ")";
}

/**
 * Generate call using a compound assignment operators ( =,+=,-=,*=,/= ).
 * Operator position is deduced from parameters type.
 * Expression is assumed to be placed just before the operator.
 *
 * \param Information about the instruction
 * \param Arguments, in their C++ form.
 * \param String to be placed at the start of the call ( the function to be
 * called typically ). Example : MyObject->Set \param Arguments will be
 * generated starting from this number. For example, set this to 1 to skip the
 * first argument.
 */
gd::String EventsCodeGenerator::GenerateCompoundOperatorCall(
    const gd::InstructionMetadata& instrInfos,
    const vector<gd::String>& arguments,
    const gd::String& callStartString,
    std::size_t startFromArgument) {
  std::size_t operatorIndex = instrInfos.parameters.size();
  for (std::size_t i = startFromArgument; i < instrInfos.parameters.size();
       ++i) {
    if (instrInfos.parameters[i].type == "operator") operatorIndex = i;
  }

  // Ensure that there is at least one parameter after the operator
  if (operatorIndex + 1 >= instrInfos.parameters.size()) {
    ReportError();
    return "";
  }

  gd::String operatorStr = arguments[operatorIndex];
  if (operatorStr.size() > 2)
    operatorStr = operatorStr.substr(
        1,
        operatorStr.length() - 1 -
            1);  // Operator contains quote which must be removed.

  gd::String rhs = arguments[operatorIndex + 1];

  // Generate real operator string.
  if (operatorStr == "+")
    operatorStr = "+=";
  else if (operatorStr == "-")
    operatorStr = "-=";
  else if (operatorStr == "/")
    operatorStr = "/=";
  else if (operatorStr == "*")
    operatorStr = "*=";

  // Generate arguments for calling the function ("setter")
  gd::String argumentsStr;
  for (std::size_t i = startFromArgument; i < arguments.size(); ++i) {
    if (i != operatorIndex &&
        i != operatorIndex + 1)  // Generate classic arguments
    {
      if (!argumentsStr.empty()) argumentsStr += ", ";
      argumentsStr += arguments[i];
    }
  }

  return callStartString + "(" + argumentsStr + ") " + operatorStr + " (" +
         rhs + ")";
}

gd::String EventsCodeGenerator::GenerateMutatorCall(
    const gd::InstructionMetadata& instrInfos,
    const vector<gd::String>& arguments,
    const gd::String& callStartString,
    std::size_t startFromArgument) {
  std::size_t operatorIndex = instrInfos.parameters.size();
  for (std::size_t i = startFromArgument; i < instrInfos.parameters.size();
       ++i) {
    if (instrInfos.parameters[i].type == "operator") operatorIndex = i;
  }

  // Ensure that there is at least one parameter after the operator
  if (operatorIndex + 1 >= instrInfos.parameters.size()) {
    ReportError();
    return "";
  }

  gd::String operatorStr = arguments[operatorIndex];
  if (operatorStr.size() > 2)
    operatorStr = operatorStr.substr(
        1,
        operatorStr.length() - 1 -
            1);  // Operator contains quote which must be removed.

  auto mutators = instrInfos.codeExtraInformation.optionalMutators;
  auto mutator = mutators.find(operatorStr);
  if (mutator == mutators.end()) {
    ReportError();
    return "";
  }

  gd::String rhs = arguments[operatorIndex + 1];

  // Generate arguments for calling the mutator
  gd::String argumentsStr;
  for (std::size_t i = startFromArgument; i < arguments.size(); ++i) {
    if (i != operatorIndex &&
        i != operatorIndex + 1)  // Generate classic arguments
    {
      if (!argumentsStr.empty()) argumentsStr += ", ";
      argumentsStr += arguments[i];
    }
  }

  return callStartString + "(" + argumentsStr + ")." + mutator->second + "(" +
         rhs + ")";
}

gd::String EventsCodeGenerator::GenerateConditionCode(
    gd::Instruction& condition,
    gd::String returnBoolean,
    EventsCodeGenerationContext& context) {
  gd::String conditionCode;

  const gd::InstructionMetadata& instrInfos =
      MetadataProvider::GetConditionMetadata(platform, condition.GetType());
  if (MetadataProvider::IsBadInstructionMetadata(instrInfos)) {
    return "/* Unknown instruction - skipped. */";
  }

  AddIncludeFiles(instrInfos.codeExtraInformation.GetIncludeFiles());
  maxConditionsListsSize =
      std::max(maxConditionsListsSize, condition.GetSubInstructions().size());

  if (instrInfos.codeExtraInformation.HasCustomCodeGenerator()) {
    context.EnterCustomCondition();
    conditionCode += GenerateReferenceToUpperScopeBoolean(
        "conditionTrue", returnBoolean, context);
    conditionCode += instrInfos.codeExtraInformation.customCodeGenerator(
        condition, *this, context);
    maxCustomConditionsDepth =
        std::max(maxCustomConditionsDepth, context.GetCurrentConditionDepth());
    context.LeaveCustomCondition();

    return "{" + conditionCode + "}\n";
  }

  // Insert code only parameters and be sure there is no lack of parameter.
  while (condition.GetParameters().size() < instrInfos.parameters.size()) {
    vector<gd::Expression> parameters = condition.GetParameters();
    parameters.push_back(gd::Expression(""));
    condition.SetParameters(parameters);
  }

  // Verify that there are no mismatchs between object type in parameters.
  for (std::size_t pNb = 0; pNb < instrInfos.parameters.size(); ++pNb) {
    if (ParameterMetadata::IsObject(instrInfos.parameters[pNb].type)) {
      gd::String objectInParameter =
          condition.GetParameter(pNb).GetPlainString();

      if (!GetObjectsAndGroups().HasObjectNamed(objectInParameter) &&
          !GetGlobalObjectsAndGroups().HasObjectNamed(objectInParameter) &&
          !GetObjectsAndGroups().GetObjectGroups().Has(objectInParameter) &&
          !GetGlobalObjectsAndGroups().GetObjectGroups().Has(
              objectInParameter)) {
        return "/* Unknown object - skipped. */";
      } else if (!instrInfos.parameters[pNb].supplementaryInformation.empty() &&
                 gd::GetTypeOfObject(GetGlobalObjectsAndGroups(),
                                     GetObjectsAndGroups(),
                                     objectInParameter) !=
                     instrInfos.parameters[pNb].supplementaryInformation) {
        return "/* Mismatched object type - skipped. */";
      }
    }
  }

  if (instrInfos.IsObjectInstruction()) {
    gd::String objectName = condition.GetParameter(0).GetPlainString();
    if (!objectName.empty() && !instrInfos.parameters.empty()) {
      std::vector<gd::String> realObjects =
          ExpandObjectsName(objectName, context);
      for (std::size_t i = 0; i < realObjects.size(); ++i) {
        // Set up the context
        gd::String objectType = gd::GetTypeOfObject(
            GetGlobalObjectsAndGroups(), GetObjectsAndGroups(), realObjects[i]);
        const ObjectMetadata& objInfo =
            MetadataProvider::GetObjectMetadata(platform, objectType);

        if (objInfo.IsUnsupportedBaseObjectCapability(
                instrInfos.GetRequiredBaseObjectCapability())) {
          conditionCode +=
              "/* Object with unsupported capability - skipped. */\n";
        } else {
          AddIncludeFiles(objInfo.includeFiles);
          context.SetCurrentObject(realObjects[i]);
          context.ObjectsListNeeded(realObjects[i]);

          // Prepare arguments and generate the condition whole code
          vector<gd::String> arguments = GenerateParametersCodes(
              condition.GetParameters(), instrInfos.parameters, context);
          conditionCode += GenerateObjectCondition(realObjects[i],
                                                   objInfo,
                                                   arguments,
                                                   instrInfos,
                                                   returnBoolean,
                                                   condition.IsInverted(),
                                                   context);

          context.SetNoCurrentObject();
        }
      }
    }
  } else if (instrInfos.IsBehaviorInstruction()) {
    gd::String objectName = condition.GetParameter(0).GetPlainString();
    gd::String behaviorType =
        gd::GetTypeOfBehavior(GetGlobalObjectsAndGroups(),
                              GetObjectsAndGroups(),
                              condition.GetParameter(1).GetPlainString());
    if (instrInfos.parameters.size() >= 2) {
      std::vector<gd::String> realObjects =
          ExpandObjectsName(objectName, context);
      for (std::size_t i = 0; i < realObjects.size(); ++i) {
        // Setup context
        const BehaviorMetadata& autoInfo =
            MetadataProvider::GetBehaviorMetadata(platform, behaviorType);
        AddIncludeFiles(autoInfo.includeFiles);
        context.SetCurrentObject(realObjects[i]);
        context.ObjectsListNeeded(realObjects[i]);

        // Prepare arguments and generate the whole condition code
        vector<gd::String> arguments = GenerateParametersCodes(
            condition.GetParameters(), instrInfos.parameters, context);
        conditionCode += GenerateBehaviorCondition(
            realObjects[i],
            condition.GetParameter(1).GetPlainString(),
            autoInfo,
            arguments,
            instrInfos,
            returnBoolean,
            condition.IsInverted(),
            context);

        context.SetNoCurrentObject();
      }
    }
  } else {
    std::vector<std::pair<gd::String, gd::String> >
        supplementaryParametersTypes;
    supplementaryParametersTypes.push_back(std::make_pair(
        "conditionInverted", condition.IsInverted() ? "true" : "false"));
    vector<gd::String> arguments =
        GenerateParametersCodes(condition.GetParameters(),
                                instrInfos.parameters,
                                context,
                                &supplementaryParametersTypes);

    conditionCode += GenerateFreeCondition(
        arguments, instrInfos, returnBoolean, condition.IsInverted(), context);
  }

  return conditionCode;
}

/**
 * Generate code for a list of conditions.
 * Bools containing conditions results are named conditionXIsTrue.
 */
gd::String EventsCodeGenerator::GenerateConditionsListCode(
    gd::InstructionsList& conditions, EventsCodeGenerationContext& context) {
  gd::String outputCode;

  for (std::size_t i = 0; i < conditions.size(); ++i)
    outputCode += GenerateBooleanInitializationToFalse(
        "condition" + gd::String::From(i) + "IsTrue", context);

  for (std::size_t cId = 0; cId < conditions.size(); ++cId) {
    gd::String conditionCode =
        GenerateConditionCode(conditions[cId],
                              "condition" + gd::String::From(cId) + "IsTrue",
                              context);
    if (!conditions[cId].GetType().empty()) {
      for (std::size_t i = 0; i < cId;
           ++i)  // Skip conditions if one condition is false. //TODO : Can be
                 // optimized
      {
        if (i == 0)
          outputCode += "if ( ";
        else
          outputCode += " && ";
        outputCode += "condition" + gd::String::From(i) + "IsTrue";
        if (i == cId - 1) outputCode += ") ";
      }

      outputCode += "{\n";
      outputCode += conditionCode;
      outputCode += "}";
    } else {
      // Deprecated way to cancel code generation - but still honor it.
      // Can be removed once condition is passed by const reference to
      // GenerateConditionCode.
      outputCode += "/* Skipped condition (empty type) */";
    }
  }

  maxConditionsListsSize = std::max(maxConditionsListsSize, conditions.size());

  return outputCode;
}

/**
 * Generate code for an action.
 */
gd::String EventsCodeGenerator::GenerateActionCode(
    gd::Instruction& action, EventsCodeGenerationContext& context) {
  gd::String actionCode;

  const gd::InstructionMetadata& instrInfos =
      MetadataProvider::GetActionMetadata(platform, action.GetType());
  if (MetadataProvider::IsBadInstructionMetadata(instrInfos)) {
    return "/* Unknown instruction - skipped. */";
  }

  AddIncludeFiles(instrInfos.codeExtraInformation.GetIncludeFiles());

  if (instrInfos.codeExtraInformation.HasCustomCodeGenerator()) {
    return instrInfos.codeExtraInformation.customCodeGenerator(
        action, *this, context);
  }

  // Be sure there is no lack of parameter.
  while (action.GetParameters().size() < instrInfos.parameters.size()) {
    vector<gd::Expression> parameters = action.GetParameters();
    parameters.push_back(gd::Expression(""));
    action.SetParameters(parameters);
  }

  // Verify that there are no mismatchs between object type in parameters.
  for (std::size_t pNb = 0; pNb < instrInfos.parameters.size(); ++pNb) {
    if (ParameterMetadata::IsObject(instrInfos.parameters[pNb].type)) {
      gd::String objectInParameter = action.GetParameter(pNb).GetPlainString();
      if (!GetObjectsAndGroups().HasObjectNamed(objectInParameter) &&
          !GetGlobalObjectsAndGroups().HasObjectNamed(objectInParameter) &&
          !GetObjectsAndGroups().GetObjectGroups().Has(objectInParameter) &&
          !GetGlobalObjectsAndGroups().GetObjectGroups().Has(
              objectInParameter)) {
        return "/* Unknown object - skipped. */";
      } else if (!instrInfos.parameters[pNb].supplementaryInformation.empty() &&
                 gd::GetTypeOfObject(GetGlobalObjectsAndGroups(),
                                     GetObjectsAndGroups(),
                                     objectInParameter) !=
                     instrInfos.parameters[pNb].supplementaryInformation) {
        return "/* Mismatched object type - skipped. */";
      }
    }
  }

  // Call free function first if available
  if (instrInfos.IsObjectInstruction()) {
    gd::String objectName = action.GetParameter(0).GetPlainString();

    if (!instrInfos.parameters.empty()) {
      std::vector<gd::String> realObjects =
          ExpandObjectsName(objectName, context);
      for (std::size_t i = 0; i < realObjects.size(); ++i) {
        // Setup context
        gd::String objectType = gd::GetTypeOfObject(
            GetGlobalObjectsAndGroups(), GetObjectsAndGroups(), realObjects[i]);
        const ObjectMetadata& objInfo =
            MetadataProvider::GetObjectMetadata(platform, objectType);

        if (objInfo.IsUnsupportedBaseObjectCapability(
                instrInfos.GetRequiredBaseObjectCapability())) {
          actionCode += "/* Object with unsupported capability - skipped. */\n";
        } else {
          AddIncludeFiles(objInfo.includeFiles);
          context.SetCurrentObject(realObjects[i]);
          context.ObjectsListNeeded(realObjects[i]);

          // Prepare arguments and generate the whole action code
          vector<gd::String> arguments = GenerateParametersCodes(
              action.GetParameters(), instrInfos.parameters, context);
          actionCode += GenerateObjectAction(
              realObjects[i], objInfo, arguments, instrInfos, context);

          context.SetNoCurrentObject();
        }
      }
    }
  } else if (instrInfos.IsBehaviorInstruction()) {
    gd::String objectName = action.GetParameter(0).GetPlainString();
    gd::String behaviorType =
        gd::GetTypeOfBehavior(GetGlobalObjectsAndGroups(),
                              GetObjectsAndGroups(),
                              action.GetParameter(1).GetPlainString());

    if (instrInfos.parameters.size() >= 2) {
      std::vector<gd::String> realObjects =
          ExpandObjectsName(objectName, context);
      for (std::size_t i = 0; i < realObjects.size(); ++i) {
        // Setup context
        const BehaviorMetadata& autoInfo =
            MetadataProvider::GetBehaviorMetadata(platform, behaviorType);
        AddIncludeFiles(autoInfo.includeFiles);
        context.SetCurrentObject(realObjects[i]);
        context.ObjectsListNeeded(realObjects[i]);

        // Prepare arguments and generate the whole action code
        vector<gd::String> arguments = GenerateParametersCodes(
            action.GetParameters(), instrInfos.parameters, context);
        actionCode +=
            GenerateBehaviorAction(realObjects[i],
                                   action.GetParameter(1).GetPlainString(),
                                   autoInfo,
                                   arguments,
                                   instrInfos,
                                   context);

        context.SetNoCurrentObject();
      }
    }
  } else {
    vector<gd::String> arguments = GenerateParametersCodes(
        action.GetParameters(), instrInfos.parameters, context);
    actionCode += GenerateFreeAction(arguments, instrInfos, context);
  }

  return actionCode;
}

/**
 * Generate actions code.
 */
gd::String EventsCodeGenerator::GenerateActionsListCode(
    gd::InstructionsList& actions, EventsCodeGenerationContext& context) {
  gd::String outputCode;
  for (std::size_t aId = 0; aId < actions.size(); ++aId) {
    gd::String actionCode = GenerateActionCode(actions[aId], context);

    outputCode += "{";
    if (actions[aId].GetType().empty()) {
      // Deprecated way to cancel code generation - but still honor it.
      // Can be removed once action is passed by const reference to
      // GenerateActionCode.
      outputCode += "/* Skipped action (empty type) */";
    } else {
      outputCode += actionCode;
    }
    outputCode += "}";
  }

  return outputCode;
}

gd::String EventsCodeGenerator::GenerateParameterCodes(
    const gd::String& parameter,
    const gd::ParameterMetadata& metadata,
    gd::EventsCodeGenerationContext& context,
    const gd::String& lastObjectName,
    std::vector<std::pair<gd::String, gd::String> >*
        supplementaryParametersTypes) {
  gd::String argOutput;

  if (ParameterMetadata::IsExpression("number", metadata.type)) {
    argOutput = gd::ExpressionCodeGenerator::GenerateExpressionCode(
        *this, context, "number", parameter);
  } else if (ParameterMetadata::IsExpression("string", metadata.type)) {
    argOutput = gd::ExpressionCodeGenerator::GenerateExpressionCode(
        *this, context, "string", parameter);
  } else if (ParameterMetadata::IsExpression("variable", metadata.type)) {
    argOutput = gd::ExpressionCodeGenerator::GenerateExpressionCode(
        *this, context, metadata.type, parameter, lastObjectName);
  } else if (ParameterMetadata::IsObject(metadata.type)) {
    // It would be possible to run a gd::ExpressionCodeGenerator if later
    // objects can have nested objects, or function returning objects.
    argOutput = GenerateObject(parameter, metadata.type, context);
  } else if (metadata.type == "relationalOperator") {
    argOutput += parameter == "=" ? "==" : parameter;
    if (argOutput != "==" && argOutput != "<" && argOutput != ">" &&
        argOutput != "<=" && argOutput != ">=" && argOutput != "!=") {
      cout << "Warning: Bad relational operator: Set to == by default." << endl;
      argOutput = "==";
    }

    argOutput = "\"" + argOutput + "\"";
  } else if (metadata.type == "operator") {
    argOutput += parameter;
    if (argOutput != "=" && argOutput != "+" && argOutput != "-" &&
        argOutput != "/" && argOutput != "*") {
      cout << "Warning: Bad operator: Set to = by default." << endl;
      argOutput = "=";
    }

    argOutput = "\"" + argOutput + "\"";
  } else if (ParameterMetadata::IsBehavior(metadata.type)) {
    argOutput = GenerateGetBehaviorNameCode(parameter);
  } else if (metadata.type == "key") {
    argOutput = "\"" + ConvertToString(parameter) + "\"";
  } else if (metadata.type == "audioResource" ||
             metadata.type == "bitmapFontResource" ||
             metadata.type == "fontResource" ||
             metadata.type == "imageResource" ||
             metadata.type == "jsonResource" ||
             metadata.type == "videoResource" ||
             // Deprecated, old parameter names:
             metadata.type == "password" || metadata.type == "musicfile" ||
             metadata.type == "soundfile" || metadata.type == "police") {
    argOutput = "\"" + ConvertToString(parameter) + "\"";
  } else if (metadata.type == "mouse") {
    argOutput = "\"" + ConvertToString(parameter) + "\"";
  } else if (metadata.type == "yesorno") {
    argOutput += (parameter == "yes" || parameter == "oui") ? GenerateTrue()
                                                            : GenerateFalse();
  } else if (metadata.type == "trueorfalse") {
    // This is duplicated in AdvancedExtension.cpp for GDJS
    argOutput += (parameter == "True" || parameter == "Vrai") ? GenerateTrue()
                                                              : GenerateFalse();
  }
  // Code only parameter type
  else if (metadata.type == "inlineCode") {
    argOutput += metadata.supplementaryInformation;
  } else {
    // Try supplementary types if provided
    if (supplementaryParametersTypes) {
      for (std::size_t i = 0; i < supplementaryParametersTypes->size(); ++i) {
        if ((*supplementaryParametersTypes)[i].first == metadata.type)
          argOutput += (*supplementaryParametersTypes)[i].second;
      }
    }

    // Type unknown
    if (argOutput.empty()) {
      if (!metadata.type.empty())
        cout << "Warning: Unknown type of parameter \"" << metadata.type
             << "\"." << std::endl;
      argOutput += "\"" + ConvertToString(parameter) + "\"";
    }
  }

  return argOutput;
}

vector<gd::String> EventsCodeGenerator::GenerateParametersCodes(
    const vector<gd::Expression>& parameters,
    const vector<gd::ParameterMetadata>& parametersInfo,
    EventsCodeGenerationContext& context,
    std::vector<std::pair<gd::String, gd::String> >*
        supplementaryParametersTypes) {
  vector<gd::String> arguments;

  gd::ParameterMetadataTools::IterateOverParameters(
      parameters,
      parametersInfo,
      [this, &context, &supplementaryParametersTypes, &arguments](
          const gd::ParameterMetadata& parameterMetadata,
          const gd::String& parameterValue,
          const gd::String& lastObjectName) {
        gd::String argOutput =
            GenerateParameterCodes(parameterValue,
                                   parameterMetadata,
                                   context,
                                   lastObjectName,
                                   supplementaryParametersTypes);
        arguments.push_back(argOutput);
      });

  return arguments;
}

gd::String EventsCodeGenerator::GenerateGetBehaviorNameCode(
    const gd::String& behaviorName) {
  return ConvertToStringExplicit(behaviorName);
}

gd::String EventsCodeGenerator::GenerateObjectsDeclarationCode(
    EventsCodeGenerationContext& context) {
  auto declareObjectList = [this](gd::String object,
                                  gd::EventsCodeGenerationContext& context) {
    gd::String objectListName = GetObjectListName(object, context);
    if (!context.GetParentContext()) {
      std::cout << "ERROR: During code generation, a context tried to use an "
                   "already declared object list without having a parent"
                << std::endl;
      return "/* Could not declare " + objectListName + " */";
    }

    //*Optimization*: Avoid a copy of the object list if we're using
    // the same list as the one from the parent context.
    if (context.IsSameObjectsList(object, *context.GetParentContext()))
      return "/* Reuse " + objectListName + " */";

    gd::String declarationCode;

    // Use a temporary variable as the names of lists are the same between
    // contexts.
    gd::String copiedListName =
        GetObjectListName(object, *context.GetParentContext());
    declarationCode += "std::vector<RuntimeObject*> & " + objectListName +
                       "T = " + copiedListName + ";\n";
    declarationCode += "std::vector<RuntimeObject*> " + objectListName + " = " +
                       objectListName + "T;\n";
    return declarationCode;
  };

  gd::String declarationsCode;
  for (auto object : context.GetObjectsListsToBeDeclared()) {
    gd::String objectListDeclaration = "";
    if (!context.ObjectAlreadyDeclared(object)) {
      objectListDeclaration = "std::vector<RuntimeObject*> " +
                              GetObjectListName(object, context) +
                              " = runtimeContext->GetObjectsRawPointers(\"" +
                              ConvertToString(object) + "\");\n";
      context.SetObjectDeclared(object);
    } else
      objectListDeclaration = declareObjectList(object, context);

    declarationsCode += objectListDeclaration + "\n";
  }
  for (auto object : context.GetObjectsListsToBeDeclaredWithoutPicking()) {
    gd::String objectListDeclaration = "";
    if (!context.ObjectAlreadyDeclared(object)) {
      objectListDeclaration = "std::vector<RuntimeObject*> " +
                              GetObjectListName(object, context) + ";\n";
      context.SetObjectDeclared(object);
    } else
      objectListDeclaration = declareObjectList(object, context);

    declarationsCode += objectListDeclaration + "\n";
  }
  for (auto object : context.GetObjectsListsToBeDeclaredEmpty()) {
    gd::String objectListDeclaration = "";
    if (!context.ObjectAlreadyDeclared(object)) {
      objectListDeclaration = "std::vector<RuntimeObject*> " +
                              GetObjectListName(object, context) + ";\n";
      context.SetObjectDeclared(object);
    } else
      objectListDeclaration = "std::vector<RuntimeObject*> " +
                              GetObjectListName(object, context) + ";\n";

    declarationsCode += objectListDeclaration + "\n";
  }

  return declarationsCode;
}

/**
 * Generate events list code.
 */
gd::String EventsCodeGenerator::GenerateEventsListCode(
    gd::EventsList& events, const EventsCodeGenerationContext& parentContext) {
  gd::String output;
  for (std::size_t eId = 0; eId < events.size(); ++eId) {
    // Each event has its own context : Objects picked in an event are totally
    // different than the one picked in another.
    gd::EventsCodeGenerationContext newContext;
    newContext.InheritsFrom(
        parentContext);  // Events in the same "level" share
                         // the same context as their parent.

    //*Optimization*: when the event is the last of a list, we can use the
    // same lists of objects as the parent (as they will be discarded just
    // after). This avoids a copy of the lists of objects which is an expensive
    // operation.
    bool reuseParentContext =
        parentContext.CanReuse() && eId == events.size() - 1;
    gd::EventsCodeGenerationContext reusedContext;
    reusedContext.Reuse(parentContext);

    auto& context = reuseParentContext ? reusedContext : newContext;

    gd::String eventCoreCode = events[eId].GenerateEventCode(*this, context);
    gd::String scopeBegin = GenerateScopeBegin(context);
    gd::String scopeEnd = GenerateScopeEnd(context);
    gd::String declarationsCode = GenerateObjectsDeclarationCode(context);

    output += "\n" + scopeBegin + "\n" + declarationsCode + "\n" +
              eventCoreCode + "\n" + scopeEnd + "\n";
  }

  return output;
}

gd::String EventsCodeGenerator::ConvertToString(gd::String plainString) {
  plainString = plainString.FindAndReplace("\\", "\\\\")
                    .FindAndReplace("\r", "\\r")
                    .FindAndReplace("\n", "\\n")
                    .FindAndReplace("\"", "\\\"");

  return plainString;
}

gd::String EventsCodeGenerator::ConvertToStringExplicit(
    gd::String plainString) {
  return "\"" + ConvertToString(plainString) + "\"";
}

std::vector<gd::String> EventsCodeGenerator::ExpandObjectsName(
    const gd::String& objectName,
    const EventsCodeGenerationContext& context) const {
  // Note: this logic is duplicated in EventsContextAnalyzer::ExpandObjectsName
  std::vector<gd::String> realObjects;
  if (globalObjectsAndGroups.GetObjectGroups().Has(objectName))
    realObjects = globalObjectsAndGroups.GetObjectGroups()
                      .Get(objectName)
                      .GetAllObjectsNames();
  else if (objectsAndGroups.GetObjectGroups().Has(objectName))
    realObjects =
        objectsAndGroups.GetObjectGroups().Get(objectName).GetAllObjectsNames();
  else
    realObjects.push_back(objectName);

  // If current object is present, use it and only it.
  if (find(realObjects.begin(),
           realObjects.end(),
           context.GetCurrentObject()) != realObjects.end()) {
    realObjects.clear();
    realObjects.push_back(context.GetCurrentObject());
  }

  // Ensure that all returned objects actually exists.
  for (std::size_t i = 0; i < realObjects.size();) {
    if (!objectsAndGroups.HasObjectNamed(realObjects[i]) &&
        !globalObjectsAndGroups.HasObjectNamed(realObjects[i]))
      realObjects.erase(realObjects.begin() + i);
    else
      ++i;
  }

  return realObjects;
}

void EventsCodeGenerator::DeleteUselessEvents(gd::EventsList& events) {
  for (std::size_t eId = events.size() - 1; eId < events.size(); --eId) {
    if (events[eId].CanHaveSubEvents())  // Process sub events, if any
      DeleteUselessEvents(events[eId].GetSubEvents());

    if (!events[eId].IsExecutable() ||
        events[eId].IsDisabled())  // Delete events that are not executable
      events.RemoveEvent(eId);
  }
}

/**
 * Call preprocessing method of each event
 */
void EventsCodeGenerator::PreprocessEventList(gd::EventsList& listEvent) {
  for (std::size_t i = 0; i < listEvent.GetEventsCount(); ++i) {
    listEvent[i].Preprocess(*this, listEvent, i);
    if (i <
        listEvent.GetEventsCount()) {  // Be sure that that there is still an
                                       // event! ( Preprocess can remove it. )
      if (listEvent[i].CanHaveSubEvents())
        PreprocessEventList(listEvent[i].GetSubEvents());
    }
  }
}

void EventsCodeGenerator::ReportError() { errorOccurred = true; }

gd::String EventsCodeGenerator::GenerateObjectFunctionCall(
    gd::String objectListName,
    const gd::ObjectMetadata& objMetadata,
    const gd::ExpressionCodeGenerationInformation& codeInfo,
    gd::String parametersStr,
    gd::String defaultOutput,
    gd::EventsCodeGenerationContext& context) {
  // To be used for testing only.
  return objectListName + "." + codeInfo.functionCallName + "(" +
         parametersStr + ") ?? " + defaultOutput;
}

gd::String EventsCodeGenerator::GenerateObjectBehaviorFunctionCall(
    gd::String objectListName,
    gd::String behaviorName,
    const gd::BehaviorMetadata& autoInfo,
    const gd::ExpressionCodeGenerationInformation& codeInfo,
    gd::String parametersStr,
    gd::String defaultOutput,
    gd::EventsCodeGenerationContext& context) {
  // To be used for testing only.
  return objectListName + "::" + behaviorName + "." +
         codeInfo.functionCallName + "(" + parametersStr + ") ?? " +
         defaultOutput;
}

gd::String EventsCodeGenerator::GenerateFreeCondition(
    const std::vector<gd::String>& arguments,
    const gd::InstructionMetadata& instrInfos,
    const gd::String& returnBoolean,
    bool conditionInverted,
    gd::EventsCodeGenerationContext& context) {
  // Generate call
  gd::String predicat;
  if (instrInfos.codeExtraInformation.type == "number" ||
      instrInfos.codeExtraInformation.type == "string") {
    predicat = GenerateRelationalOperatorCall(
        instrInfos,
        arguments,
        instrInfos.codeExtraInformation.functionCallName);
  } else {
    predicat = instrInfos.codeExtraInformation.functionCallName + "(" +
               GenerateArgumentsList(arguments, 0) + ")";
  }

  // Add logical not if needed
  bool conditionAlreadyTakeCareOfInversion = false;
  for (std::size_t i = 0; i < instrInfos.parameters.size();
       ++i)  // Some conditions already have a "conditionInverted" parameter
  {
    if (instrInfos.parameters[i].type == "conditionInverted")
      conditionAlreadyTakeCareOfInversion = true;
  }
  if (!conditionAlreadyTakeCareOfInversion && conditionInverted)
    predicat = GenerateNegatedPredicat(predicat);

  // Generate condition code
  return returnBoolean + " = " + predicat + ";\n";
}

gd::String EventsCodeGenerator::GenerateObjectCondition(
    const gd::String& objectName,
    const gd::ObjectMetadata& objInfo,
    const std::vector<gd::String>& arguments,
    const gd::InstructionMetadata& instrInfos,
    const gd::String& returnBoolean,
    bool conditionInverted,
    gd::EventsCodeGenerationContext& context) {
  // Prepare call
  // Add a static_cast if necessary
  gd::String objectFunctionCallNamePart =
      (!instrInfos.parameters[0].supplementaryInformation.empty())
          ? "static_cast<" + objInfo.className + "*>(" +
                GetObjectListName(objectName, context) + "[i])->" +
                instrInfos.codeExtraInformation.functionCallName
          : GetObjectListName(objectName, context) + "[i]->" +
                instrInfos.codeExtraInformation.functionCallName;

  // Create call
  gd::String predicat;
  if ((instrInfos.codeExtraInformation.type == "number" ||
       instrInfos.codeExtraInformation.type == "string")) {
    predicat = GenerateRelationalOperatorCall(
        instrInfos, arguments, objectFunctionCallNamePart, 1);
  } else {
    predicat = objectFunctionCallNamePart + "(" +
               GenerateArgumentsList(arguments, 1) + ")";
  }
  if (conditionInverted) predicat = GenerateNegatedPredicat(predicat);

  return "For each picked object \"" + objectName + "\", check " + predicat +
         ".\n";
}

gd::String EventsCodeGenerator::GenerateBehaviorCondition(
    const gd::String& objectName,
    const gd::String& behaviorName,
    const gd::BehaviorMetadata& autoInfo,
    const std::vector<gd::String>& arguments,
    const gd::InstructionMetadata& instrInfos,
    const gd::String& returnBoolean,
    bool conditionInverted,
    gd::EventsCodeGenerationContext& context) {
  // Create call
  gd::String predicat;
  if ((instrInfos.codeExtraInformation.type == "number" ||
       instrInfos.codeExtraInformation.type == "string")) {
    predicat = GenerateRelationalOperatorCall(instrInfos, arguments, "", 2);
  } else {
    predicat = "(" + GenerateArgumentsList(arguments, 2) + ")";
  }
  if (conditionInverted) predicat = GenerateNegatedPredicat(predicat);

  return "For each picked object \"" + objectName + "\", check " + predicat +
         " for behavior \"" + behaviorName + "\".\n";
}

gd::String EventsCodeGenerator::GenerateFreeAction(
    const std::vector<gd::String>& arguments,
    const gd::InstructionMetadata& instrInfos,
    gd::EventsCodeGenerationContext& context) {
  // Generate call
  gd::String call;
  if (instrInfos.codeExtraInformation.type == "number" ||
      instrInfos.codeExtraInformation.type == "string") {
    if (instrInfos.codeExtraInformation.accessType ==
        gd::InstructionMetadata::ExtraInformation::MutatorAndOrAccessor)
      call = GenerateOperatorCall(
          instrInfos,
          arguments,
          instrInfos.codeExtraInformation.functionCallName,
          instrInfos.codeExtraInformation.optionalAssociatedInstruction);
    else if (instrInfos.codeExtraInformation.accessType ==
             gd::InstructionMetadata::ExtraInformation::Mutators)
      call =
          GenerateMutatorCall(instrInfos,
                              arguments,
                              instrInfos.codeExtraInformation.functionCallName);
    else
      call = GenerateCompoundOperatorCall(
          instrInfos,
          arguments,
          instrInfos.codeExtraInformation.functionCallName);
  } else {
    call = instrInfos.codeExtraInformation.functionCallName + "(" +
           GenerateArgumentsList(arguments) + ")";
  }
  return call + ";\n";
}

gd::String EventsCodeGenerator::GenerateObjectAction(
    const gd::String& objectName,
    const gd::ObjectMetadata& objInfo,
    const std::vector<gd::String>& arguments,
    const gd::InstructionMetadata& instrInfos,
    gd::EventsCodeGenerationContext& context) {
  // Create call
  gd::String call;
  if ((instrInfos.codeExtraInformation.type == "number" ||
       instrInfos.codeExtraInformation.type == "string")) {
    if (instrInfos.codeExtraInformation.accessType ==
        gd::InstructionMetadata::ExtraInformation::MutatorAndOrAccessor)
      call = GenerateOperatorCall(
          instrInfos,
          arguments,
          instrInfos.codeExtraInformation.functionCallName,
          instrInfos.codeExtraInformation.optionalAssociatedInstruction,
          2);
    else
      call = GenerateCompoundOperatorCall(
          instrInfos,
          arguments,
          instrInfos.codeExtraInformation.functionCallName,
          2);

    return "For each picked object \"" + objectName + "\", call " + call +
           ".\n";
  } else {
    gd::String argumentsStr = GenerateArgumentsList(arguments, 1);

    call = instrInfos.codeExtraInformation.functionCallName + "(" +
           argumentsStr + ")";
    return "For each picked object \"" + objectName + "\", call " + call + "(" +
           argumentsStr + ").\n";
  }
}

gd::String EventsCodeGenerator::GenerateBehaviorAction(
    const gd::String& objectName,
    const gd::String& behaviorName,
    const gd::BehaviorMetadata& autoInfo,
    const std::vector<gd::String>& arguments,
    const gd::InstructionMetadata& instrInfos,
    gd::EventsCodeGenerationContext& context) {
  // Create call
  gd::String call;
  if ((instrInfos.codeExtraInformation.type == "number" ||
       instrInfos.codeExtraInformation.type == "string")) {
    if (instrInfos.codeExtraInformation.accessType ==
        gd::InstructionMetadata::ExtraInformation::MutatorAndOrAccessor)
      call = GenerateOperatorCall(
          instrInfos,
          arguments,
          instrInfos.codeExtraInformation.functionCallName,
          instrInfos.codeExtraInformation.optionalAssociatedInstruction,
          2);
    else
      call = GenerateCompoundOperatorCall(
          instrInfos,
          arguments,
          instrInfos.codeExtraInformation.functionCallName,
          2);
    return "For each picked object \"" + objectName + "\", call " + call +
           " for behavior \"" + behaviorName + "\".\n";
  } else {
    gd::String argumentsStr = GenerateArgumentsList(arguments, 2);

    call = instrInfos.codeExtraInformation.functionCallName + "(" +
           argumentsStr + ")";
    return "For each picked object \"" + objectName + "\", call " + call + "(" +
           argumentsStr + ")" + " for behavior \"" + behaviorName + "\".\n";
  }
}

size_t EventsCodeGenerator::GenerateSingleUsageUniqueIdForEventsList() {
  return eventsListNextUniqueId++;
}

size_t EventsCodeGenerator::GenerateSingleUsageUniqueIdFor(
    const Instruction* instruction) {
  if (!instruction) {
    std::cout << "ERROR: During code generation, a null pointer was passed to "
                 "GenerateSingleUsageUniqueIdFor."
              << std::endl;
  }

  // Base the unique id on the adress in memory so that the same instruction
  // in memory will get the same id across different code generations.
  size_t uniqueId = (size_t)instruction;

  // While in most case this function is called a single time for each
  // instruction, it's possible for an instruction to be appearing more than
  // once in the events, if we used links. In this case, simply increment the
  // unique id to be sure that ids are effectively uniques, and stay stable
  // (given the same order of links).
  while (instructionUniqueIds.find(uniqueId) != instructionUniqueIds.end()) {
    uniqueId++;
  }
  instructionUniqueIds.insert(uniqueId);
  return uniqueId;
}

gd::String EventsCodeGenerator::GetObjectListName(
    const gd::String& name, const gd::EventsCodeGenerationContext& context) {
  return ManObjListName(name);
}

gd::String EventsCodeGenerator::GenerateArgumentsList(
    const std::vector<gd::String>& arguments, size_t startFrom) {
  gd::String argumentsStr;
  for (std::size_t i = startFrom; i < arguments.size(); ++i) {
    if (!argumentsStr.empty()) argumentsStr += ", ";
    argumentsStr += arguments[i];
  }

  return argumentsStr;
}

EventsCodeGenerator::EventsCodeGenerator(gd::Project& project_,
                                         const gd::Layout& layout,
                                         const gd::Platform& platform_)
    : platform(platform_),
      globalObjectsAndGroups(project_),
      objectsAndGroups(layout),
      hasProjectAndLayout(true),
      project(&project_),
      scene(&layout),
      errorOccurred(false),
      compilationForRuntime(false),
      maxCustomConditionsDepth(0),
      maxConditionsListsSize(0),
      eventsListNextUniqueId(0){};

EventsCodeGenerator::EventsCodeGenerator(
    const gd::Platform& platform_,
    gd::ObjectsContainer& globalObjectsAndGroups_,
    const gd::ObjectsContainer& objectsAndGroups_)
    : platform(platform_),
      globalObjectsAndGroups(globalObjectsAndGroups_),
      objectsAndGroups(objectsAndGroups_),
      hasProjectAndLayout(false),
      project(nullptr),
      scene(nullptr),
      errorOccurred(false),
      compilationForRuntime(false),
      maxCustomConditionsDepth(0),
      maxConditionsListsSize(0),
      eventsListNextUniqueId(0){};

}  // namespace gd
