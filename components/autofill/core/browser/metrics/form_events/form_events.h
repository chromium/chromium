// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENTS_H_

namespace autofill::autofill_metrics {

// Form Events for autofill.
// These events are triggered separately for address and credit card forms.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum FormEvent {
  // User interacted with a field of this kind of form. Logged only once per
  // page load.
  FORM_EVENT_INTERACTED_ONCE = 0,
  // A dropdown with suggestions was shown.
  FORM_EVENT_SUGGESTIONS_SHOWN = 1,
  // Same as above, but recoreded only once per page load.
  FORM_EVENT_SUGGESTIONS_SHOWN_ONCE = 2,
  // A local suggestion was used to fill the form.
  FORM_EVENT_LOCAL_SUGGESTION_FILLED = 3,
  // A server suggestion was used to fill the form.
  // When dealing with credit cards, this means a full server card was used
  // to fill.
  //
  // Deprecated as full server cards are no longer offered as suggestions.
  // FORM_EVENT_SERVER_SUGGESTION_FILLED = 4,
  // A masked server card suggestion was used to fill the form.
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED = 5,
  // A suggestion was used to fill the form. The origin type (local or masked
  // server card) of the first selected within a page load will determine which
  // of the following will be fired. VIRTUAL_CARD is also an option later in the
  // enum list.
  //
  // Full server cards are no longer offered as suggestions so the corresponding
  // enum entry is deprecated.
  FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE = 6,
  // FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE = 7,
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE = 8,
  // A form was submitted. Depending on the user filling a local, server,
  // masked server card, no suggestion, or virtual card (later in the enum
  // list), one of the following will be triggered. Only one of the following
  // four or virtual card will be triggered per page load.
  //
  // Full server cards are no longer offered as suggestions so the corresponding
  // enum entry is deprecated.
  FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE = 9,
  FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE = 10,
  // FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE = 11,
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE = 12,
  // DEPRECATED IN M123, DO NOT USE. Use value 87 instead!
  // A masked server card suggestion was selected to fill the form.
  DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED = 13,
  // DEPRECATED IN M123, DO NOT USE. Use value 88 instead!
  // Same as above but only triggered once per page load.
  DEPRECATED_FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE = 14,
  // An autofillable form is about to be submitted. If the submission is not
  // interrupted by JavaScript, the "form submitted" events above will also be
  // logged. Depending on the user filling a local, server, masked server card,
  // no suggestion, or virtual card (later in the enum list), one of the
  // following will be triggered, at most once per page load.
  //
  // Full server cards are no longer offered as suggestions so the corresponding
  // enum entry is deprecated.
  FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE = 15,
  FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE = 16,
  // FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE = 17,
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE = 18,
  // A dropdown with suggestions was shown and a form was submitted after
  // that.
  FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE = 19,
  // A dropdown with suggestions was shown and a form is about to be
  // submitted. If the submission is not interrupted by JavaScript, the "form
  // submitted" event above will also be logged.
  FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE = 20,
  // A dropdown with credit card suggestions was shown, but they were not used
  // to fill the form. Depending on the user submitting a card known by the
  // browser, submitting a card that the browser does not know about,
  // submitting with an empty card number, submitting with a card number of
  // wrong size or submitting with a card number that does not pass luhn
  // check, one of the following will be triggered. At most one of the
  // following five metrics will be triggered per submit.
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD = 21,
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD = 22,
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD = 23,
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_WRONG_SIZE_CARD = 24,
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_FAIL_LUHN_CHECK_CARD = 25,

  // The form was changed dynamically. This value has been deprecated.
  // DEPRECATED_FORM_EVENT_DID_SEE_DYNAMIC_FORM = 26,

  // The form was changed dynamically and was fillable.
  // DEPRECATED_FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM = 27,

  // There was a dynamic change of the form and it got re-filled
  // automatically.
  FORM_EVENT_DID_DYNAMIC_REFILL = 28,

  // The form dynamically changed another time after the refill.
  // DEPRECATED_FORM_EVENT_DYNAMIC_CHANGE_AFTER_REFILL = 29,

  // The two events below are deprecated and no longer emitted.
  FORM_EVENT_POPUP_SUPPRESSED = 30,
  FORM_EVENT_POPUP_SUPPRESSED_ONCE = 31,

  // The form was parsed.
  FORM_EVENT_DID_PARSE_FORM = 32,

  // The user selected the "Hide Suggestions" item.
  FORM_EVENT_USER_HIDE_SUGGESTIONS = 33,
  // Same as above, but recorded only once per page load.
  FORM_EVENT_USER_HIDE_SUGGESTIONS_ONCE = 34,

  // A virtual card suggestion was selected to fill the form.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED = 35,
  // Same as above, but recorded only once per page load.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE = 36,
  // A virtual card suggestion was used to fill the form.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED = 37,
  // Same as above, but recorded only once per page load.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE = 38,
  // An Autofillable form is about to be submitted, after being filled with a
  // virtual card.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE = 39,
  // A form was submitted after being filled with a virtual card.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE = 40,
  // Suggestions were shown, and they included a virtual credit card.
  FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD = 41,
  // Same as above, but recorded only once per page load.
  FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE = 42,

  // See AutofillMetrics::CreditCardSeamlessFillMetric for details.
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_FULL_FILL = 43,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_OPTIONAL_NAME_MISSING = 44,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_OPTIONAL_CVC_MISSING = 45,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_OPTIONAL_NAME_AND_CVC_MISSING = 46,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_FULL_FILL_BUT_EXPDATE_MISSING = 47,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILL_PARTIAL_FILL = 48,

  // A cross-origin fill was prevented only because shared-autofill was disabled
  // in the field's frame. Shared-autofill is a policy-controlled feature by
  // which a frame can allow a child-frame to be autofilled across origin.
  FORM_EVENT_CREDIT_CARD_MISSING_SHARED_AUTOFILL = 49,

  // See AutofillMetrics::CreditCardSeamlessness::Metric for details.
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_FULL_FILL = 50,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_OPTIONAL_NAME_MISSING = 51,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_OPTIONAL_CVC_MISSING = 52,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_OPTIONAL_NAME_AND_CVC_MISSING = 53,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_FULL_FILL_BUT_EXPDATE_MISSING = 54,
  FORM_EVENT_CREDIT_CARD_SEAMLESS_FILLABLE_PARTIAL_FILL = 55,

  // Metric logged when a local card with a matching deduplicated server
  // suggestion was filled.
  FORM_EVENT_LOCAL_SUGGESTION_FILLED_FOR_AN_EXISTING_SERVER_CARD_ONCE = 56,

  // A field of the form was cleared by Javascript within kLimitBeforeRefill
  // after being autofilled. Recorded once per form.
  FORM_EVENT_AUTOFILLED_FIELD_CLEARED_BY_JAVASCRIPT_AFTER_FILL_ONCE = 57,

  // Credit card suggestions were shown, and it included at least one suggestion
  // with metadata.
  FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN = 58,
  // Credit card suggestions were shown, and none had metadata.
  FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN = 59,
  // The selected credit card suggestion had metadata.
  FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED = 60,
  // The selected credit card suggestion did not have metadata.
  FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED = 61,

  // Reserved so that the serialized DenseSet<FormEvent> are representable as
  // int64_t. Such conversions uint64_t -> int64_t happen when serializing a
  // DenseSet<FormEvent> and passing it to a UKM builder.
  FORM_EVENT_RESERVED_NOT_FOR_USE = 63,

  // Credit card suggestions were shown, and they included at least one
  // suggestion with metadata. Logged once per page load.
  FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN_ONCE = 64,
  // Credit card suggestions were shown, and none had metadata. Logged once per
  // page load.
  FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN_ONCE = 65,
  // The selected credit card suggestion had metadata. Logged once per page
  // load.
  FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED_ONCE = 66,
  // The selected credit card suggestion did not have metadata. Logged once per
  // page load.
  FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED_ONCE = 67,
  // The filled credit card suggestion had metadata.
  FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED = 68,
  // The filled credit card suggestion had no metadata.
  FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED = 69,
  // A credit card was submitted after a suggestion was filled,
  // and the suggested card had metadata. Logged once per page load.
  FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SUBMITTED_ONCE = 70,
  // A credit card was submitted after a suggestion was filled,
  // and the suggested card had no metadata. Logged once per page load.
  FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SUBMITTED_ONCE = 71,

  // Metric logged when a server card with a matching deduplicated local
  // suggestion was filled.
  FORM_EVENT_SERVER_CARD_SUGGESTION_SELECTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE =
      72,
  FORM_EVENT_SERVER_CARD_FILLED_FOR_AN_EXISTING_LOCAL_CARD_ONCE = 73,
  FORM_EVENT_SERVER_CARD_SUBMITTED_FOR_AN_EXISTING_LOCAL_CARD_ONCE = 74,

  // The filled credit card suggestion had metadata. Logged once per
  // page load.
  FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED_ONCE = 75,
  // The filled credit card suggestion had no metadata. Logged once per
  // page load.
  FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED_ONCE = 76,
  // A credit card was about to be submitted after a suggestion was filled,
  // and the suggested card had metadata. Logged once per page load.
  FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_WILL_SUBMIT_ONCE = 77,
  // A credit card was about to be submitted after a suggestion was filled,
  // and the suggested card had no metadata. Logged once per page load.
  FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_WILL_SUBMIT_ONCE = 78,

  // Metric logged when a credit card suggestion with cvc info was shown.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN = 79,
  // Metric logged when a credit card suggestion with cvc info was shown. Logged
  // once per page load.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SHOWN_ONCE = 80,
  // Metric logged when a credit card suggestion with cvc info was selected.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED = 81,
  // Metric logged when a credit card suggestion with cvc info was selected.
  // Logged once per page load.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SELECTED_ONCE = 82,
  // Metric logged when a credit card suggestion with cvc info was filled.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED = 83,
  // Metric logged when a credit card suggestion with cvc info was filled.
  // Logged once per page load.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_FILLED_ONCE = 84,
  // Metric logged when form is about to be submitted after a credit card
  // suggestion with cvc info was filled. Logged once per page load.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_WILL_SUBMIT_ONCE = 85,
  // Metric logged when form was submitted after a credit card suggestion with
  // cvc info was filled. Logged once per page load.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_CVC_SUBMITTED_ONCE = 86,

  // A masked server card suggestion was selected to fill the form. Updated in
  // M123 to include a fix for missing selection events.
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED = 87,
  // Same as above but only triggered once per page load.
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE = 88,

  // Suggestions containing cards with a benefit available were shown.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_BENEFIT_AVAILABLE_SHOWN = 89,
  // DEPRECATED in M128, DO NOT USE.
  // Suggestions were shown, and no card had a benefit available.
  DEPRECATED_FORM_EVENT_SUGGESTION_FOR_CARD_WITHOUT_BENEFIT_AVAILABLE_SHOWN =
      90,
  // Suggestions containing cards with a benefit available were shown. Logged
  // once per page load.
  FORM_EVENT_SUGGESTION_FOR_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE = 91,
  // DEPRECATED in M128, DO NOT USE.
  // Suggestions were shown, and no card had a benefit available. Logged once
  // per page load.
  DEPRECATED_FORM_EVENT_SUGGESTION_FOR_CARD_WITHOUT_BENEFIT_AVAILABLE_SHOWN_ONCE =
      92,
  // A suggestion of a masked server card with a benefit available was
  // selected.
  FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_SELECTED = 93,
  // DEPRECATED in M128, DO NOT USE.
  // A suggestion of a masked server card with no benefit available was
  // selected.
  DEPRECATED_FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITHOUT_BENEFIT_AVAILABLE_SELECTED =
      94,
  // A suggestion of a masked server card with a benefit available was
  // selected. Logged once per page load.
  FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_SELECTED_ONCE =
      95,
  // DEPRECATED in M128, DO NOT USE.
  // A suggestion of a masked server card with no benefit available was
  // selected. Logged once per page load.
  DEPRECATED_FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITHOUT_BENEFIT_AVAILABLE_SELECTED_ONCE =
      96,
  // A suggestion of a masked server card with a benefit available was filled.
  FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_FILLED = 97,
  // DEPRECATED in M128, DO NOT USE.
  // A suggestion of a masked server card with no benefit available was filled.
  DEPRECATED_FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITHOUT_BENEFIT_AVAILABLE_FILLED =
      98,
  // A suggestion of a masked server card with a benefit available was filled.
  // Logged once per page load.
  FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_FILLED_ONCE = 99,
  // DEPRECATED in M128, DO NOT USE.
  // A suggestion of a masked server card with no benefit available was filled.
  // Logged once per page load.
  DEPRECATED_FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITHOUT_BENEFIT_AVAILABLE_FILLED_ONCE =
      100,
  // DEPRECATED in M128, DO NOT USE. Duplicated with 103.
  // A form was about to be submitted after a suggestion of a masked server
  // card with a benefit available was filled. Logged once per page load.
  DEPRECATED_FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_WILL_SUBMIT_ONCE =
      101,
  // DEPRECATED in M128, DO NOT USE.
  // A form was about to be submitted after a suggestion of a masked server
  // card with no benefit available was filled. Logged once per page load.
  DEPRECATED_FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITHOUT_BENEFIT_AVAILABLE_WILL_SUBMIT_ONCE =
      102,
  // A form was submitted after a suggestion of a masked server card with
  // benefit available was filled. Logged once per page load.
  FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_SUBMITTED_ONCE =
      103,
  // DEPRECATED in M128, DO NOT USE.
  // A form was submitted after a suggestion of a masked server card with no
  // benefit available was filled. Logged once per page load.
  DEPRECATED_FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITHOUT_BENEFIT_AVAILABLE_SUBMITTED_ONCE =
      104,
  // A masked server card suggestion is selected after suggestions containing
  // cards with a benefit available were shown. The selected card can be any
  // masked server card, even it has no available benefit. Logged once per
  // page load.
  FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_SELECTED_AFTER_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE =
      105,
  // A masked server card suggestion is filled after suggestions containing
  // cards with a benefit available were shown. The filled card can be any
  // masked server card, even it has no available benefit. Logged once per
  // page load.
  FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_FILLED_AFTER_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE =
      106,
  // A form is submitted after suggestions containing cards with a benefit
  // available were shown and a masked server card suggestion was filled.
  // The filled card can be any masked server card, even it has no available
  // benefit. Logged once per page load.
  FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_SUBMITTED_AFTER_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE =
      107,

  // A local card suggestion was selected to fill the form.
  FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED = 108,
  // Same as above but only triggered once per page load.
  FORM_EVENT_LOCAL_CARD_SUGGESTION_SELECTED_ONCE = 109,

  NUM_FORM_EVENTS,
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENTS_H_
