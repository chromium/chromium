// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/log_event.h"

#include "base/notreached.h"

namespace autofill {

FillEventId GetNextFillEventId() {
  static FillEventId::Generator fill_event_id_generator;
  return fill_event_id_generator.GenerateNextId();
}

OptionalBoolean& operator|=(OptionalBoolean& a, OptionalBoolean b) {
  if (b == OptionalBoolean::kTrue || a == OptionalBoolean::kUndefined) {
    a = b;
  }

  return a;
}

OptionalBoolean ToOptionalBoolean(bool value) {
  return static_cast<OptionalBoolean>(value);
}

bool OptionalBooleanToBool(OptionalBoolean value) {
  switch (value) {
    case OptionalBoolean::kFalse:
      return false;
    case OptionalBoolean::kTrue:
      return true;
    case OptionalBoolean::kUndefined:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool AreCollapsible(const absl::monostate& event1,
                    const absl::monostate& event2) {
  return true;
}

bool AreCollapsible(const AskForValuesToFillFieldLogEvent& event1,
                    const AskForValuesToFillFieldLogEvent& event2) {
  return event1.has_suggestion == event2.has_suggestion &&
         event1.suggestion_is_shown == event2.suggestion_is_shown;
}

bool AreCollapsible(const TriggerFillFieldLogEvent& event1,
                    const TriggerFillFieldLogEvent& event2) {
  return event1.fill_event_id == event2.fill_event_id;
}

bool AreCollapsible(const FillFieldLogEvent& event1,
                    const FillFieldLogEvent& event2) {
  return event1.fill_event_id == event2.fill_event_id &&
         event1.had_value_before_filling == event2.had_value_before_filling &&
         event1.autofill_skipped_status == event2.autofill_skipped_status &&
         event1.was_autofilled_before_security_policy ==
             event2.was_autofilled_before_security_policy &&
         event1.had_value_after_filling == event2.had_value_after_filling &&
         event1.filling_method == event2.filling_method &&
         event1.filling_prevented_by_iframe_security_policy ==
             event2.filling_prevented_by_iframe_security_policy;
}

bool AreCollapsible(const TypingFieldLogEvent& event1,
                    const TypingFieldLogEvent& event2) {
  return event1.has_value_after_typing == event2.has_value_after_typing;
}

bool AreCollapsible(const HeuristicPredictionFieldLogEvent& event1,
                    const HeuristicPredictionFieldLogEvent& event2) {
  return event1.field_type == event2.field_type &&
         event1.heuristic_source == event2.heuristic_source &&
         event1.is_active_heuristic_source ==
             event2.is_active_heuristic_source &&
         event1.rank_in_field_signature_group ==
             event2.rank_in_field_signature_group;
}

bool AreCollapsible(const AutocompleteAttributeFieldLogEvent& event1,
                    const AutocompleteAttributeFieldLogEvent& event2) {
  return event1.html_type == event2.html_type &&
         event1.html_mode == event2.html_mode &&
         event1.rank_in_field_signature_group ==
             event2.rank_in_field_signature_group;
}

bool AreCollapsible(const ServerPredictionFieldLogEvent& event1,
                    const ServerPredictionFieldLogEvent& event2) {
  return event1.server_type1 == event2.server_type1 &&
         event1.prediction_source1 == event2.prediction_source1 &&
         event1.server_type2 == event2.server_type2 &&
         event1.prediction_source2 == event2.prediction_source2 &&
         event1.server_type_prediction_is_override ==
             event2.server_type_prediction_is_override &&
         event1.rank_in_field_signature_group ==
             event2.rank_in_field_signature_group;
}

bool AreCollapsible(const RationalizationFieldLogEvent& event1,
                    const RationalizationFieldLogEvent& event2) {
  return event1.field_type == event2.field_type &&
         event1.section_id == event2.section_id &&
         event1.type_changed == event2.type_changed;
}

bool AreCollapsible(const AblationFieldLogEvent& event1,
                    const AblationFieldLogEvent& event2) {
  return event1.ablation_group == event2.ablation_group &&
         event1.conditional_ablation_group ==
             event2.conditional_ablation_group &&
         event1.day_in_ablation_window == event2.day_in_ablation_window;
}

}  // namespace autofill
