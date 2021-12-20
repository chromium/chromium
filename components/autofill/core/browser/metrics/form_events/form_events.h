// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENTS_H_

namespace autofill {

// Form Events for autofill.
// These events are triggered separately for address and credit card forms.
enum FormEvent {
  // User interacted with a field of this kind of form. Logged only once per
  // page load.
  FORM_EVENT_INTERACTED_ONCE = 0,
  // A dropdown with suggestions was shown.
  FORM_EVENT_SUGGESTIONS_SHOWN,
  // Same as above, but recoreded only once per page load.
  FORM_EVENT_SUGGESTIONS_SHOWN_ONCE,
  // A local suggestion was used to fill the form.
  FORM_EVENT_LOCAL_SUGGESTION_FILLED,
  // A server suggestion was used to fill the form.
  // When dealing with credit cards, this means a full server card was used
  // to fill.
  FORM_EVENT_SERVER_SUGGESTION_FILLED,
  // A masked server card suggestion was used to fill the form.
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED,
  // A suggestion was used to fill the form. The origin type (local or server
  // or masked server card) of the first selected within a page load will
  // determine which of the following will be fired. VIRTUAL_CARD is also an
  // option later in the enum list.
  FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
  FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE,
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE,
  // A form was submitted. Depending on the user filling a local, server,
  // masked server card, no suggestion, or virtual card (later in the enum
  // list), one of the following will be triggered. Only one of the following
  // four or virtual card will be triggered per page load.
  FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE,
  FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE,
  FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE,
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
  // A masked server card suggestion was selected to fill the form.
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
  // Same as above but only triggered once per page load.
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
  // An autofillable form is about to be submitted. If the submission is not
  // interrupted by JavaScript, the "form submitted" events above will also be
  // logged. Depending on the user filling a local, server, masked server card,
  // no suggestion, or virtual card (later in the enum list), one of the
  // following will be triggered, at most once per page load.
  FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE,
  FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE,
  FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE,
  FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
  // A dropdown with suggestions was shown and a form was submitted after
  // that.
  FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE,
  // A dropdown with suggestions was shown and a form is about to be
  // submitted. If the submission is not interrupted by JavaScript, the "form
  // submitted" event above will also be logged.
  FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE,
  // A dropdown with credit card suggestions was shown, but they were not used
  // to fill the form. Depending on the user submitting a card known by the
  // browser, submitting a card that the browser does not know about,
  // submitting with an empty card number, submitting with a card number of
  // wrong size or submitting with a card number that does not pass luhn
  // check, one of the following will be triggered. At most one of the
  // following five metrics will be triggered per submit.
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD,
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD,
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD,
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_WRONG_SIZE_CARD,
  FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_FAIL_LUHN_CHECK_CARD,

  // The form was changed dynamically. This value has been deprecated.
  FORM_EVENT_DID_SEE_DYNAMIC_FORM,
  // The form was changed dynamically and was fillable.
  FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM,
  // There was a dynamic change of the form and it got re-filled
  // automatically.
  FORM_EVENT_DID_DYNAMIC_REFILL,
  // The form dynamically changed another time after the refill.
  FORM_EVENT_DYNAMIC_CHANGE_AFTER_REFILL,
  // The popup was suppressed because the native view couldn't be created.
  FORM_EVENT_POPUP_SUPPRESSED,
  // Same as above, but recoreded only once per page load.
  FORM_EVENT_POPUP_SUPPRESSED_ONCE,

  // The form was parsed.
  FORM_EVENT_DID_PARSE_FORM,

  // The user selected the "Hide Suggestions" item.
  FORM_EVENT_USER_HIDE_SUGGESTIONS,
  // Same as above, but recorded only once per page load.
  FORM_EVENT_USER_HIDE_SUGGESTIONS_ONCE,

  // A virtual card suggestion was selected to fill the form.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED,
  // Same as above, but recorded only once per page load.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED_ONCE,
  // A virtual card suggestion was used to fill the form.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED,
  // Same as above, but recorded only once per page load.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_FILLED_ONCE,
  // An Autofillable form is about to be submitted, after being filled with a
  // virtual card.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
  // A form was submitted after being filled with a virtual card.
  FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SUBMITTED_ONCE,
  // Suggestions were shown, and they included a virtual credit card.
  FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD,
  // Same as above, but recorded only once per page load.
  FORM_EVENT_SUGGESTIONS_SHOWN_WITH_VIRTUAL_CARD_ONCE,

  // See AutofillMetrics::CreditCardSeamlessFillMetric for details.
  FORM_EVENT_CREDIT_CARD_SEAMLESSNESS_FULL_FILL,
  FORM_EVENT_CREDIT_CARD_SEAMLESSNESS_OPTIONAL_NAME_MISSING,
  FORM_EVENT_CREDIT_CARD_SEAMLESSNESS_OPTIONAL_CVC_MISSING,
  FORM_EVENT_CREDIT_CARD_SEAMLESSNESS_OPTIONAL_NAME_AND_CVC_MISSING,
  FORM_EVENT_CREDIT_CARD_SEAMLESSNESS_FULL_FILL_BUT_EXPDATE_MISSING,
  FORM_EVENT_CREDIT_CARD_SEAMLESSNESS_PARTIAL_FILL,

  NUM_FORM_EVENTS,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_FORM_EVENTS_FORM_EVENTS_H_
