// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/trigger_script.h"

namespace autofill_assistant {

namespace {

bool EvaluateTriggerCondition(
    const TriggerScriptConditionProto& proto,
    const StaticTriggerConditions& static_trigger_conditions,
    const DynamicTriggerConditions& dynamic_trigger_conditions) {
  switch (proto.type_case()) {
    case TriggerScriptConditionProto::kAllOf: {
      for (const auto& condition : proto.all_of().conditions()) {
        if (!EvaluateTriggerCondition(condition, static_trigger_conditions,
                                      dynamic_trigger_conditions)) {
          return false;
        }
      }
      return true;
    }
    case TriggerScriptConditionProto::kAnyOf: {
      for (const auto& condition : proto.any_of().conditions()) {
        if (EvaluateTriggerCondition(condition, static_trigger_conditions,
                                     dynamic_trigger_conditions)) {
          return true;
        }
      }
      return false;
    }
    case TriggerScriptConditionProto::kNoneOf: {
      for (const auto& condition : proto.none_of().conditions()) {
        if (EvaluateTriggerCondition(condition, static_trigger_conditions,
                                     dynamic_trigger_conditions)) {
          return false;
        }
      }
      return true;
    }
    case TriggerScriptConditionProto::kSelector: {
      auto selector_matches = dynamic_trigger_conditions.GetSelectorMatches(
          Selector(proto.selector()));
      DCHECK(selector_matches.has_value());
      return selector_matches.value();
    }
    case TriggerScriptConditionProto::kStoredLoginCredentials:
      return static_trigger_conditions.has_stored_login_credentials();
    case TriggerScriptConditionProto::kIsFirstTimeUser:
      return static_trigger_conditions.is_first_time_user();
    case TriggerScriptConditionProto::kExperimentId:
      return static_trigger_conditions.is_in_experiment(proto.experiment_id());
    case TriggerScriptConditionProto::kKeyboardHidden:
      return !dynamic_trigger_conditions.GetKeyboardVisible();
    case TriggerScriptConditionProto::kScriptParameterMatch:
      return static_trigger_conditions.script_parameter_matches(
          proto.script_parameter_match());
    case TriggerScriptConditionProto::kPathPattern:
      return dynamic_trigger_conditions.GetPathPatternMatches(
          proto.path_pattern());
    case TriggerScriptConditionProto::kDomainWithScheme:
      return dynamic_trigger_conditions.GetDomainAndSchemeMatches(
          GURL(proto.domain_with_scheme()));
    case TriggerScriptConditionProto::kDocumentReadyState: {
      absl::optional<DocumentReadyState> state =
          dynamic_trigger_conditions.GetDocumentReadyState(
              Selector(proto.document_ready_state().frame()));
      return state.has_value() &&
             *state >= proto.document_ready_state().min_document_ready_state();
    }
    case TriggerScriptConditionProto::TYPE_NOT_SET:
      return true;
  }
}

}  // namespace

TriggerScript::TriggerScript(const TriggerScriptProto& proto) : proto_(proto) {}
TriggerScript::~TriggerScript() = default;

bool TriggerScript::EvaluateTriggerConditions(
    const StaticTriggerConditions& static_trigger_conditions,
    const DynamicTriggerConditions& dynamic_trigger_conditions) const {
  if (!proto_.has_trigger_condition()) {
    return true;
  }
  return EvaluateTriggerCondition(proto_.trigger_condition(),
                                  static_trigger_conditions,
                                  dynamic_trigger_conditions);
}

TriggerScriptProto TriggerScript::AsProto() const {
  return proto_;
}

bool TriggerScript::waiting_for_precondition_no_longer_true() const {
  return waiting_for_precondition_no_longer_true_;
}

void TriggerScript::waiting_for_precondition_no_longer_true(bool waiting) {
  waiting_for_precondition_no_longer_true_ = waiting;
}

}  // namespace autofill_assistant
