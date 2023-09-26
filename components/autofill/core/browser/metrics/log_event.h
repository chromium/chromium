// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_LOG_EVENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_LOG_EVENT_H_

#include "base/time/time.h"
#include "base/types/id_type.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

// An identifier to connect the various sub-events of filling together.
using FillEventId = base::IdTypeU32<class FillEventIdClass>;
FillEventId GetNextFillEventId();

enum class OptionalBoolean {
  kFalse = 0,
  kTrue = 1,
  kUndefined = 2,
};
OptionalBoolean& operator|=(OptionalBoolean& a, OptionalBoolean b);
OptionalBoolean ToOptionalBoolean(bool value);
bool OptionalBooleanToBool(OptionalBoolean value);

// Whether and why filling for a field was skipped during autofill.
enum class FieldFillingSkipReason {
  // Values are recorded as metrics and must not change or be reused.
  kUnknown = 0,
  kNotSkipped = 1,
  kNotInFilledSection = 2,
  kNotFocused = 3,
  kFormChanged = 4,
  kInvisibleField = 5,
  kValuePrefilled = 6,
  kUserFilledFields = 7,
  kAutofilledFieldsNotRefill = 8,
  kNoFillableGroup = 9,
  kRefillNotInInitialFill = 10,
  kExpiredCards = 11,
  kFillingLimitReachedType = 12,
  kUnrecognizedAutocompleteAttribute = 13,
  kFieldDoesNotMatchTargetFieldsSet = 14,
  kMaxValue = kFieldDoesNotMatchTargetFieldsSet
};

// Enum for different data types filled during autofill filling events,
// including those of the SingleFieldFormFiller.
// Values are recorded as metrics and must not change or be reused.
enum class FillDataType {
  kUndefined = 0,
  kAutofillProfile = 1,
  kCreditCard = 2,
  kSingleFieldFormFillerAutocomplete = 3,
  kSingleFieldFormFillerIban = 4,
  kSingleFieldFormFillerPromoCode = 5,
};

// AreCollapsible(..., ...) are a set of functions that checks whether two
// consecutive events in the event log of a form can be merged into one.
// This is a best effort mechanism to reduce the memory footprint caused by
// redundant events.
bool AreCollapsible(const absl::monostate& event1,
                    const absl::monostate& event2);

// Log the field that shows a dropdown list of suggestions for autofill.
template <typename IsRequired = void>
struct AskForValuesToFillFieldLogEventImpl {
  OptionalBoolean has_suggestion = IsRequired();
  OptionalBoolean suggestion_is_shown = IsRequired();
};
using AskForValuesToFillFieldLogEvent = AskForValuesToFillFieldLogEventImpl<>;

bool AreCollapsible(const AskForValuesToFillFieldLogEvent& event1,
                    const AskForValuesToFillFieldLogEvent& event2);

// Log the field that triggers the suggestion that the user selects to fill.
template <typename IsRequired = void>
struct TriggerFillFieldLogEventImpl {
  FillEventId fill_event_id = GetNextFillEventId();
  // The type of filled data for the autofil event.
  FillDataType data_type = IsRequired();
  // The country_code associated with the information filled. Only present for
  // autofill addresses (i.e. `AutofillEventType::kAutofillProfile`).
  std::string associated_country_code = IsRequired();
  // The time at which the event occurred.
  base::Time timestamp = IsRequired();
};
using TriggerFillFieldLogEvent = TriggerFillFieldLogEventImpl<>;

bool AreCollapsible(const TriggerFillFieldLogEvent& event1,
                    const TriggerFillFieldLogEvent& event2);

// Log the fields on the form that are autofilled.
template <typename IsRequired = void>
struct FillFieldLogEventImpl {
  // This refers to `TriggleFillFieldLogEvent::fill_event_id`.
  FillEventId fill_event_id = IsRequired();
  OptionalBoolean had_value_before_filling = IsRequired();
  FieldFillingSkipReason autofill_skipped_status = IsRequired();
  // The two attributes below are only valid if |autofill_skipped_status| has a
  // value of "kNotSkipped".
  // Whether the field was autofilled during this fill operation. If a fill
  // operation did not change the value of a field because the old value
  // matches the filled value, this is still recorded as a
  // was_autofilled = true.
  OptionalBoolean was_autofilled = IsRequired();
  // Whether the field had a value after this fill operation.
  OptionalBoolean had_value_after_filling = IsRequired();
};
using FillFieldLogEvent = FillFieldLogEventImpl<>;

bool AreCollapsible(const FillFieldLogEvent& event1,
                    const FillFieldLogEvent& event2);

// Log the field that the user types in.
template <typename IsRequired = void>
struct TypingFieldLogEventImpl {
  OptionalBoolean has_value_after_typing = IsRequired();
};
using TypingFieldLogEvent = TypingFieldLogEventImpl<>;

bool AreCollapsible(const TypingFieldLogEvent& event1,
                    const TypingFieldLogEvent& event2);

// Events recorded after local heuristic prediction happened.
template <typename IsRequired = void>
struct HeuristicPredictionFieldLogEventImpl {
  ServerFieldType field_type = IsRequired();
  PatternSource pattern_source = IsRequired();
  bool is_active_pattern_source = IsRequired();
  size_t rank_in_field_signature_group = IsRequired();
};
using HeuristicPredictionFieldLogEvent = HeuristicPredictionFieldLogEventImpl<>;

bool AreCollapsible(const HeuristicPredictionFieldLogEvent& event1,
                    const HeuristicPredictionFieldLogEvent& event2);

// Events recorded after parsing autocomplete attribute.
template <typename IsRequired = void>
struct AutocompleteAttributeFieldLogEventImpl {
  HtmlFieldType html_type = IsRequired();
  HtmlFieldMode html_mode = IsRequired();
  size_t rank_in_field_signature_group = IsRequired();
};
using AutocompleteAttributeFieldLogEvent =
    AutocompleteAttributeFieldLogEventImpl<>;

bool AreCollapsible(const AutocompleteAttributeFieldLogEvent& event1,
                    const AutocompleteAttributeFieldLogEvent& event2);

// Events recorded after autofill server prediction happened.
template <typename IsRequired = void>
struct ServerPredictionFieldLogEventImpl {
  ServerFieldType server_type1 = IsRequired();
  FieldPrediction::Source prediction_source1 = IsRequired();
  ServerFieldType server_type2 = IsRequired();
  FieldPrediction::Source prediction_source2 = IsRequired();
  bool server_type_prediction_is_override = IsRequired();
  size_t rank_in_field_signature_group = IsRequired();
};
using ServerPredictionFieldLogEvent = ServerPredictionFieldLogEventImpl<>;

bool AreCollapsible(const ServerPredictionFieldLogEvent& event1,
                    const ServerPredictionFieldLogEvent& event2);

// Events recorded after rationalization happened.
template <typename IsRequired = void>
struct RationalizationFieldLogEventImpl {
  ServerFieldType field_type = IsRequired();
  size_t section_id = IsRequired();
  bool type_changed = IsRequired();
};
using RationalizationFieldLogEvent = RationalizationFieldLogEventImpl<>;

bool AreCollapsible(const RationalizationFieldLogEvent& event1,
                    const RationalizationFieldLogEvent& event2);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_LOG_EVENT_H_
