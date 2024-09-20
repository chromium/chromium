// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_LOG_EVENT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_LOG_EVENT_H_

#include "base/time/time.h"
#include "base/types/id_type.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/form_filler.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/common/is_required.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

// An identifier to connect the various sub-events of filling together.
using FillEventId = base::IdTypeU32<class FillEventIdClass>;
FillEventId GetNextFillEventId();

enum class OptionalBoolean : uint8_t {
  kFalse = 0,
  kTrue = 1,
  kUndefined = 2,
};
OptionalBoolean& operator|=(OptionalBoolean& a, OptionalBoolean b);
OptionalBoolean ToOptionalBoolean(bool value);
bool OptionalBooleanToBool(OptionalBoolean value);

// Enum for different data types filled during autofill filling events,
// including those of the SingleFieldFormFiller.
// Values are recorded as metrics and must not change or be reused.
enum class FillDataType : uint8_t {
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
struct AskForValuesToFillFieldLogEvent {
  OptionalBoolean has_suggestion = internal::IsRequired();
  OptionalBoolean suggestion_is_shown = internal::IsRequired();
};

bool AreCollapsible(const AskForValuesToFillFieldLogEvent& event1,
                    const AskForValuesToFillFieldLogEvent& event2);

// Log the field that triggers the suggestion that the user selects to fill.
struct TriggerFillFieldLogEvent {
  FillEventId fill_event_id = GetNextFillEventId();
  // The type of filled data for the Autofill event.
  FillDataType data_type = internal::IsRequired();
  // The country_code associated with the information filled. Only present for
  // autofill addresses (i.e. `AutofillEventType::kAutofillProfile`).
  std::string associated_country_code = internal::IsRequired();
  // The time at which the event occurred.
  base::Time timestamp = internal::IsRequired();
};

bool AreCollapsible(const TriggerFillFieldLogEvent& event1,
                    const TriggerFillFieldLogEvent& event2);

// Log the fields on the form that are autofilled.
struct FillFieldLogEvent {
  // This refers to `TriggleFillFieldLogEvent::fill_event_id`.
  FillEventId fill_event_id = internal::IsRequired();
  OptionalBoolean had_value_before_filling = internal::IsRequired();
  FieldFillingSkipReason autofill_skipped_status = internal::IsRequired();
  // The two attributes below are only valid if |autofill_skipped_status| has a
  // value of "kNotSkipped".
  // Whether the field was autofilled during this fill operation. If a fill
  // operation did not change the value of a field because the old value
  // matches the filled value, this is still recorded as a
  // was_autofilled = true before checking the iframe security policy.
  OptionalBoolean was_autofilled_before_security_policy =
      internal::IsRequired();
  // Whether the field had a value after this fill operation.
  OptionalBoolean had_value_after_filling = internal::IsRequired();
  // The `FillingMethod` used to fill the field. This represents the
  // different popup surfaces a user can use to interact with Autofill, which
  // may lead to a different set of fields being filled. These sets/groups can
  // be either the full form, a group of related fields or a single field.
  FillingMethod filling_method = FillingMethod::kNone;
  // Records whether filling was ever prevented because of the cross c
  // autofill security policy that applies to credit cards.
  OptionalBoolean filling_prevented_by_iframe_security_policy =
      OptionalBoolean::kUndefined;
  // The hash of the value that would have been filled if the field wasn't
  // skipped because it was pre-filled on page load. In all other cases this
  // member is set to `std::nullopt`.
  std::optional<size_t>
      value_that_would_have_been_filled_in_a_prefilled_field_hash =
          std::nullopt;
};

bool AreCollapsible(const FillFieldLogEvent& event1,
                    const FillFieldLogEvent& event2);

// Log the field that the user types in.
struct TypingFieldLogEvent {
  OptionalBoolean has_value_after_typing = internal::IsRequired();
};

bool AreCollapsible(const TypingFieldLogEvent& event1,
                    const TypingFieldLogEvent& event2);

// Events recorded after local heuristic prediction happened.
struct HeuristicPredictionFieldLogEvent {
  FieldType field_type = internal::IsRequired();
  HeuristicSource heuristic_source = internal::IsRequired();
  bool is_active_heuristic_source = internal::IsRequired();
  size_t rank_in_field_signature_group = internal::IsRequired();
};

bool AreCollapsible(const HeuristicPredictionFieldLogEvent& event1,
                    const HeuristicPredictionFieldLogEvent& event2);

// Events recorded after parsing autocomplete attribute.
struct AutocompleteAttributeFieldLogEvent {
  HtmlFieldType html_type = internal::IsRequired();
  HtmlFieldMode html_mode = internal::IsRequired();
  size_t rank_in_field_signature_group = internal::IsRequired();
};

bool AreCollapsible(const AutocompleteAttributeFieldLogEvent& event1,
                    const AutocompleteAttributeFieldLogEvent& event2);

// Events recorded after autofill server prediction happened.
struct ServerPredictionFieldLogEvent {
  std::optional<FieldType> server_type1 =
      static_cast<FieldType>(internal::IsRequired());
  FieldPrediction::Source prediction_source1 = internal::IsRequired();
  std::optional<FieldType> server_type2 =
      static_cast<FieldType>(internal::IsRequired());
  FieldPrediction::Source prediction_source2 = internal::IsRequired();
  bool server_type_prediction_is_override = internal::IsRequired();
  size_t rank_in_field_signature_group = internal::IsRequired();
};

bool AreCollapsible(const ServerPredictionFieldLogEvent& event1,
                    const ServerPredictionFieldLogEvent& event2);

// Events recorded after rationalization happened.
struct RationalizationFieldLogEvent {
  FieldType field_type = internal::IsRequired();
  size_t section_id = internal::IsRequired();
  bool type_changed = internal::IsRequired();
};

bool AreCollapsible(const RationalizationFieldLogEvent& event1,
                    const RationalizationFieldLogEvent& event2);

struct AblationFieldLogEvent {
  AblationGroup ablation_group = internal::IsRequired();
  AblationGroup conditional_ablation_group = internal::IsRequired();
  int day_in_ablation_window = internal::IsRequired();
};

bool AreCollapsible(const AblationFieldLogEvent& event1,
                    const AblationFieldLogEvent& event2);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_LOG_EVENT_H_
