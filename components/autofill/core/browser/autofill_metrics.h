// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_

#include <stddef.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/security_state/core/security_state.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace autofill {

class AutofillField;
class CreditCard;
enum class SubmissionSource;

class AutofillMetrics {
 public:
  enum AutofillProfileAction {
    EXISTING_PROFILE_USED,
    EXISTING_PROFILE_UPDATED,
    NEW_PROFILE_CREATED,
    AUTOFILL_PROFILE_ACTION_ENUM_SIZE,
  };

  enum AutofillFormSubmittedState {
    NON_FILLABLE_FORM_OR_NEW_DATA,
    FILLABLE_FORM_AUTOFILLED_ALL,
    FILLABLE_FORM_AUTOFILLED_SOME,
    FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS,
    FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS,
    AUTOFILL_FORM_SUBMITTED_STATE_ENUM_SIZE,
  };

  enum class BillingIdStatus {
    MISSING = 0,
    PARSE_ERROR = 1,
    VALID = 2,
    kMaxValue = VALID,
  };

  enum CardUploadDecisionMetric {
    // All the required conditions were satisfied using either the form fields
    // or we prompted the user to fix one or more conditions in the card upload
    // prompt.
    UPLOAD_OFFERED = 1 << 0,
    // CVC field was not found in the form.
    CVC_FIELD_NOT_FOUND = 1 << 1,
    // CVC field was found, but field did not have a value.
    CVC_VALUE_NOT_FOUND = 1 << 2,
    // CVC field had a value, but it was not valid for the card network.
    INVALID_CVC_VALUE = 1 << 3,
    // A field had a syntactically valid CVC value, but it was in a field that
    // was not heuristically determined as |CREDIT_CARD_VERIFICATION_CODE|.
    // Set only if |CVC_FIELD_NOT_FOUND| is not set.
    FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD = 1 << 4,
    // No address profile was available.
    // We don't know whether we would have been able to get upload details.
    UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE = 1 << 5,
    // Found one or more address profiles but none were recently modified or
    // recently used -i.e. not used in expected duration of a checkout flow.
    // We don't know whether we would have been able to get upload details.
    UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS = 1 << 6,
    // One or more recently used addresses were available but no zip code was
    // found on any of the address(es). We don't know whether we would have
    // been able to get upload details.
    UPLOAD_NOT_OFFERED_NO_ZIP_CODE = 1 << 7,
    // Multiple recently used addresses were available but the addresses had
    // conflicting zip codes.We don't know whether we would have been able to
    // get upload details.
    UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS = 1 << 8,
    // One or more recently used addresses were available but no name was found
    // on either the card or the address(es). We don't know whether the
    // address(es) were otherwise valid nor whether we would have been able to
    // get upload details.
    UPLOAD_NOT_OFFERED_NO_NAME = 1 << 9,
    // One or more recently used addresses were available but the names on the
    // card and/or the addresses didn't match. We don't know whether the
    // address(es) were otherwise valid nor whether we would have been able to
    // get upload details.
    UPLOAD_NOT_OFFERED_CONFLICTING_NAMES = 1 << 10,
    // One or more valid addresses, and a name were available but the request to
    // Payments for upload details failed.
    UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED = 1 << 11,
    // A textfield for the user to enter/confirm cardholder name was surfaced
    // in the offer-to-save dialog.
    USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME = 1 << 12,
    // The Autofill StrikeDatabase decided not to allow offering to save for
    // this card. On mobile, that means no save prompt is shown at all.
    UPLOAD_NOT_OFFERED_MAX_STRIKES_ON_MOBILE = 1 << 13,
    // Update |kNumCardUploadDecisionMetrics| when adding new enum here.
  };

  enum DeveloperEngagementMetric {
    // Parsed a form that is potentially autofillable and does not contain any
    // web developer-specified field type hint.
    FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS = 0,
    // Parsed a form that is potentially autofillable and contains at least one
    // web developer-specified field type hint, a la
    // http://is.gd/whatwg_autocomplete
    FILLABLE_FORM_PARSED_WITH_TYPE_HINTS,
    // Parsed a form that is potentially autofillable and contains at least one
    // UPI Virtual Payment Address hint (upi-vpa)
    FORM_CONTAINS_UPI_VPA_HINT,
    NUM_DEVELOPER_ENGAGEMENT_METRICS,
  };

  enum InfoBarMetric {
    INFOBAR_SHOWN = 0,  // We showed an infobar, e.g. prompting to save credit
                        // card info.
    INFOBAR_ACCEPTED,   // The user explicitly accepted the infobar.
    INFOBAR_DENIED,     // The user explicitly denied the infobar.
    INFOBAR_IGNORED,    // The user completely ignored the infobar (logged on
                        // tab close).
    INFOBAR_NOT_SHOWN_INVALID_LEGAL_MESSAGE,  // We didn't show the infobar
                                              // because the provided legal
                                              // message was invalid.
    NUM_INFO_BAR_METRICS,
  };

  // Represents card submitted state.
  enum SubmittedCardStateMetric {
    // Submitted card has valid card number and expiration date.
    HAS_CARD_NUMBER_AND_EXPIRATION_DATE,
    // Submitted card has a valid card number but an invalid or missing
    // expiration date.
    HAS_CARD_NUMBER_ONLY,
    // Submitted card has a valid expiration date but an invalid or missing card
    // number.
    HAS_EXPIRATION_DATE_ONLY,
    NUM_SUBMITTED_CARD_STATE_METRICS,
  };

  // Metric to measure if a submitted card's expiration date matches the same
  // server card's expiration date (unmasked or not).  Cards are considered to
  // be the same if they have the same card number (if unmasked) or if they have
  // the same network and last four digits (if masked).
  enum SubmittedServerCardExpirationStatusMetric {
    // The submitted card and the unmasked server card had the same expiration
    // date.
    FULL_SERVER_CARD_EXPIRATION_DATE_MATCHED,
    // The submitted card and the unmasked server card had different expiration
    // dates.
    FULL_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH,
    // The submitted card and the masked server card had the same expiration
    // date.
    MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED,
    // The submitted card and the masked server card had different expiration
    // dates.
    MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH,
    NUM_SUBMITTED_SERVER_CARD_EXPIRATION_STATUS_METRICS,
  };

  // Metric to distinguish between local credit card saves and upload credit
  // card saves.
  enum class SaveTypeMetric {
    LOCAL = 0,
    SERVER = 1,
    kMaxValue = SERVER,
  };

  // Metric to measure volume of cards that are disallowed for upload by their
  // network, most likely due to their network being blocked by Google Payments.
  enum UploadDisallowedForNetworkMetric {
    DISALLOWED_ELO = 0,
    DISALLOWED_JCB = 1,
    kMaxValue = DISALLOWED_JCB,
  };

  // Metric to measure if a card for which upload was offered is already stored
  // as a local card on the device or if it has not yet been seen.
  enum UploadOfferedCardOriginMetric {
    // Credit card upload was offered for a local card already on the device.
    OFFERING_UPLOAD_OF_LOCAL_CARD,
    // Credit card upload was offered for a newly-seen credit card.
    OFFERING_UPLOAD_OF_NEW_CARD,
    NUM_UPLOAD_OFFERED_CARD_ORIGIN_METRICS,
  };

  // Metric to measure if a card for which upload was accepted is already stored
  // as a local card on the device or if it has not yet been seen.
  enum UploadAcceptedCardOriginMetric {
    // The user accepted upload of a local card already on the device.
    USER_ACCEPTED_UPLOAD_OF_LOCAL_CARD,
    // The user accepted upload of a newly-seen credit card.
    USER_ACCEPTED_UPLOAD_OF_NEW_CARD,
    NUM_UPLOAD_ACCEPTED_CARD_ORIGIN_METRICS,
  };

  // Metrics to measure user interaction with the save credit card prompt.
  //
  // SAVE_CARD_PROMPT_DISMISS_FOCUS is not stored explicitly, but can be
  // inferred from the other metrics:
  // SAVE_CARD_PROMPT_DISMISS_FOCUS = SHOW_REQUESTED - END_* - DISMISS_*
  enum SaveCardPromptMetric {
    // Prompt was requested to be shown due to:
    // CC info being submitted (first show), or
    // location bar icon being clicked while bubble is hidden (reshows).
    SAVE_CARD_PROMPT_SHOW_REQUESTED,
    // The prompt was shown successfully.
    SAVE_CARD_PROMPT_SHOWN,
    // The prompt was not shown because the legal message was invalid.
    SAVE_CARD_PROMPT_END_INVALID_LEGAL_MESSAGE,
    // The user explicitly accepted the prompt.
    SAVE_CARD_PROMPT_END_ACCEPTED,
    // The user explicitly denied the prompt.
    SAVE_CARD_PROMPT_END_DENIED,
    // The prompt and icon were removed because of navigation away from the
    // page that caused the prompt to be shown. The navigation occurred while
    // the prompt was showing.
    SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING,
    // The prompt and icon were removed because of navigation away from the
    // page that caused the prompt to be shown. The navigation occurred while
    // the prompt was hidden.
    SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN,
    // The prompt was dismissed because the user clicked the "Learn more" link.
    // Deprecated.
    DEPRECATED_SAVE_CARD_PROMPT_DISMISS_CLICK_LEARN_MORE,
    // The prompt was dismissed because the user clicked a legal message link.
    SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE,

    // The following _CVC_FIX_FLOW_ metrics are independent of the ones above,
    // and were relevant when the CVC fix flow was active M62-M64. During that
    // time, for instance, accepting the CVC fix flow would trigger both
    // SAVE_CARD_PROMPT_CVC_FIX_FLOW_END_ACCEPTED as well as
    // SAVE_CARD_PROMPT_END_ACCEPTED.  They were split apart in order to track
    // acceptance/abandonment rates of the multi-stage dialog user experience.
    // (SAVE_CARD_PROMPT_CVC_FIX_FLOW_END_DENIED was an impossible state because
    // the CVC fix flow uses a close button instead of a cancel button.)

    // The prompt moved to a second stage that requested CVC from the user.
    SAVE_CARD_PROMPT_CVC_FIX_FLOW_SHOWN,
    // The user explicitly entered CVC and accepted the prompt.
    SAVE_CARD_PROMPT_CVC_FIX_FLOW_END_ACCEPTED,
    // The prompt and icon were removed because of navigation away from the page
    // that caused the prompt to be shown.  The navigation occurred while the
    // prompt was showing, at the CVC request stage.
    SAVE_CARD_PROMPT_CVC_FIX_FLOW_END_NAVIGATION_SHOWING,
    // The prompt and icon were removed because of navigation away from the page
    // that caused the prompt to be shown.  The navigation occurred while the
    // prompt was hidden, at the CVC request stage.
    SAVE_CARD_PROMPT_CVC_FIX_FLOW_END_NAVIGATION_HIDDEN,
    // The prompt was dismissed because the user clicked a legal message link.
    SAVE_CARD_PROMPT_CVC_FIX_FLOW_DISMISS_CLICK_LEGAL_MESSAGE,

    NUM_SAVE_CARD_PROMPT_METRICS,
  };

  // Metrics to measure user interaction with the Manage Cards view
  // shown when user clicks on the save card icon after accepting
  // to save a card.
  enum ManageCardsPromptMetric {
    // The manage cards promo was shown.
    MANAGE_CARDS_SHOWN,
    // The user clicked on [Done].
    MANAGE_CARDS_DONE,
    // The user clicked on [Manage cards].
    MANAGE_CARDS_MANAGE_CARDS,

    NUM_MANAGE_CARDS_PROMPT_METRICS
  };

  // Metrics measuring how well we predict field types.  These metric values are
  // logged for each field in a submitted form for:
  //     - the heuristic prediction
  //     - the crowd-sourced (server) prediction
  //     - for the overall prediction
  //
  // For each of these prediction types, these metrics are also logged by
  // actual and predicted field type.
  enum FieldTypeQualityMetric {
    // The field was found to be of type T, which matches the predicted type.
    // i.e. actual_type == predicted type == T
    //
    // This is captured as a type-specific log entry for T. Is is also captured
    // as an aggregate (non-type-specific) log entry.
    TRUE_POSITIVE = 0,

    // The field type is AMBIGUOUS and autofill made no prediction.
    // i.e. actual_type == AMBIGUOUS,predicted type == UNKNOWN|NO_SERVER_DATA.
    //
    // This is captured as an aggregate (non-type-specific) log entry. It is
    // NOT captured by type-specific logging.
    TRUE_NEGATIVE_AMBIGUOUS,

    // The field type is UNKNOWN and autofill made no prediction.
    // i.e. actual_type == UNKNOWN and predicted type == UNKNOWN|NO_SERVER_DATA.
    //
    // This is captured as an aggregate (non-type-specific) log entry. It is
    // NOT captured by type-specific logging.
    TRUE_NEGATIVE_UNKNOWN,

    // The field type is EMPTY and autofill predicted UNKNOWN
    // i.e. actual_type == EMPTY and predicted type == UNKNOWN|NO_SERVER_DATA.
    //
    // This is captured as an aggregate (non-type-specific) log entry. It is
    // NOT captured by type-specific logging.
    TRUE_NEGATIVE_EMPTY,

    // Autofill predicted type T, but the field actually had a different type.
    // i.e., actual_type == T, predicted_type = U, T != U,
    //       UNKNOWN not in (T,U).
    //
    // This is captured as a type-specific log entry for U. It is NOT captured
    // as an aggregate (non-type-specific) entry as this would double count with
    // FALSE_NEGATIVE_MISMATCH logging captured for T.
    FALSE_POSITIVE_MISMATCH,

    // Autofill predicted type T, but the field actually matched multiple
    // pieces of autofill data, none of which are T.
    // i.e., predicted_type == T, actual_type = {U, V, ...),
    //       T not in {U, V, ...}.
    //
    // This is captured as a type-specific log entry for T. It is also captured
    // as an aggregate (non-type-specific) log entry.
    FALSE_POSITIVE_AMBIGUOUS,

    // The field type is UNKNOWN, but autofill predicted it to be of type T.
    // i.e., actual_type == UNKNOWN, predicted_type = T, T != UNKNOWN
    //
    // This is captured as a type-specific log entry for T. Is is also captured
    // as an aggregate (non-type-specific) log entry.
    FALSE_POSITIVE_UNKNOWN,

    // The field type is EMPTY, but autofill predicted it to be of type T.
    // i.e., actual_type == EMPTY, predicted_type = T, T != UNKNOWN
    //
    // This is captured as a type-specific log entry for T. Is is also captured
    // as an aggregate (non-type-specific) log entry.
    FALSE_POSITIVE_EMPTY,

    // The field is of type T, but autofill did not make a type prediction.
    // i.e., actual_type == T, predicted_type = UNKNOWN, T != UNKNOWN.
    //
    // This is captured as a type-specific log entry for T. Is is also captured
    // as an aggregate (non-type-specific) log entry.
    FALSE_NEGATIVE_UNKNOWN,

    // The field is of type T, but autofill predicted it to be of type U.
    // i.e., actual_type == T, predicted_type = U, T != U,
    //       UNKNOWN not in (T,U).
    //
    // This is captured as a type-specific log entry for T. Is is also captured
    // as an aggregate (non-type-specific) log entry.
    FALSE_NEGATIVE_MISMATCH,

    // This must be last.
    NUM_FIELD_TYPE_QUALITY_METRICS
  };

  // Metrics measuring how well rationalization has performed given user's
  // actual input.
  enum RationalizationQualityMetric {
    // Rationalization did make it better for the user. Most commonly, user
    // have left it empty as rationalization predicted.
    RATIONALIZATION_GOOD,

    // Rationalization did not make it better or worse. Meaning user have
    // input some value that would not be filled correctly automatically.
    RATIONALIZATION_OK,

    // Rationalization did make it worse, user has to fill
    // in a value that would have been automatically filled
    // if there was no rationalization at all.
    RATIONALIZATION_BAD,

    // This must be last.
    NUM_RATIONALIZATION_QUALITY_METRICS
  };

  enum QualityMetricPredictionSource {
    PREDICTION_SOURCE_UNKNOWN,    // Not used. The prediction source is unknown.
    PREDICTION_SOURCE_HEURISTIC,  // Local heuristic field-type prediction.
    PREDICTION_SOURCE_SERVER,     // Crowd-sourced server field type prediction.
    PREDICTION_SOURCE_OVERALL,    // Overall field-type prediction seen by user.
    NUM_QUALITY_METRIC_SOURCES
  };

  enum QualityMetricType {
    TYPE_SUBMISSION = 0,      // Logged based on user's submitted data.
    TYPE_NO_SUBMISSION,       // Logged based on user's entered data.
    TYPE_AUTOCOMPLETE_BASED,  // Logged based on the value of autocomplete attr.
    NUM_QUALITY_METRIC_TYPES,
  };

  // Each of these is logged at most once per query to the server, which in turn
  // occurs at most once per page load.
  enum ServerQueryMetric {
    QUERY_SENT = 0,           // Sent a query to the server.
    QUERY_RESPONSE_RECEIVED,  // Received a response.
    QUERY_RESPONSE_PARSED,    // Successfully parsed the server response.

    // The response was parseable, but provided no improvements relative to our
    // heuristics.
    QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS,

    // Our heuristics detected at least one auto-fillable field, and the server
    // response overrode the type of at least one field.
    QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS,

    // Our heuristics did not detect any auto-fillable fields, but the server
    // response did detect at least one.
    QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS,
    NUM_SERVER_QUERY_METRICS,
  };

  // Logs usage of "Scan card" control item.
  enum ScanCreditCardPromptMetric {
    // "Scan card" was presented to the user.
    SCAN_CARD_ITEM_SHOWN,
    // "Scan card" was selected by the user.
    SCAN_CARD_ITEM_SELECTED,
    // The user selected something in the dropdown besides "scan card".
    SCAN_CARD_OTHER_ITEM_SELECTED,
    NUM_SCAN_CREDIT_CARD_PROMPT_METRICS,
  };

  // Metrics to track events when local credit card migration is offered.
  enum LocalCardMigrationBubbleOfferMetric {
    // The bubble is requested due to a credit card being used or
    // local card migration icon in the omnibox being clicked.
    LOCAL_CARD_MIGRATION_BUBBLE_REQUESTED = 0,
    // The bubble is actually shown to the user.
    LOCAL_CARD_MIGRATION_BUBBLE_SHOWN = 1,
    NUM_LOCAL_CARD_MIGRATION_BUBBLE_OFFER_METRICS,
  };

  // Metrics to track user interactions with the bubble.
  enum LocalCardMigrationBubbleUserInteractionMetric {
    // The user explicitly accepts the offer.
    LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_ACCEPTED = 0,
    // The user explicitly denies the offer (clicks the cancel button).
    LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_DENIED = 1,
    // The bubble is closed due to user navigating away from the page
    // while the bubble was showing.
    LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_NAVIGATED_WHILE_SHOWING = 2,
    // The bubble is closed due to user navigating away from the page
    // while the bubble was hidden.
    LOCAL_CARD_MIGRATION_BUBBLE_CLOSED_NAVIGATED_WHILE_HIDDEN = 3,
    NUM_LOCAL_CARD_MIGRATION_BUBBLE_USER_INTERACTION_METRICS,
  };

  // Metrics to track events when local card migration dialog is offered.
  enum LocalCardMigrationDialogOfferMetric {
    // The dialog is shown to the user.
    LOCAL_CARD_MIGRATION_DIALOG_SHOWN = 0,
    // The dialog is not shown due to legal message being invalid.
    LOCAL_CARD_MIGRATION_DIALOG_NOT_SHOWN_INVALID_LEGAL_MESSAGE = 1,
    NUM_LOCAL_CARD_MIGRATION_DIALOG_OFFER_METRICS,
  };

  // Metrics to track user interactions with the dialog.
  enum LocalCardMigrationDialogUserInteractionMetric {
    // The user explicitly accepts the offer by clicking the save button.
    LOCAL_CARD_MIGRATION_DIALOG_CLOSED_SAVE_BUTTON_CLICKED = 0,
    // The user explicitly denies the offer by clicking the cancel button.
    LOCAL_CARD_MIGRATION_DIALOG_CLOSED_CANCEL_BUTTON_CLICKED = 1,
    // The user clicks the legal message.
    LOCAL_CARD_MIGRATION_DIALOG_LEGAL_MESSAGE_CLICKED = 2,
    NUM_LOCAL_CARD_MIGRATION_DIALOG_USER_INTERACTION_METRICS,
  };

  // These metrics are logged for each local card migration origin. These are
  // used to derive the conversion rate for each triggering source.
  enum LocalCardMigrationPromptMetric {
    // The intermediate bubble is shown to the user.
    INTERMEDIATE_BUBBLE_SHOWN = 0,
    // The intermediate bubble is accepted by the user.
    INTERMEDIATE_BUBBLE_ACCEPTED = 1,
    // The main dialog is shown to the user.
    MAIN_DIALOG_SHOWN = 2,
    // The main dialog is accepted by the user.
    MAIN_DIALOG_ACCEPTED = 3,
    NUM_LOCAL_CARD_MIGRATION_PROMPT_METRICS,
  };

  // Local card migration origin denotes from where the migration is triggered.
  enum LocalCardMigrationOrigin {
    // Trigger when user submitted a form using local card.
    UseOfLocalCard,
    // Trigger when user submitted a form using server card.
    UseOfServerCard,
    // Trigger from settings page.
    SettingsPage,
  };

  // Each of these metrics is logged only for potentially autofillable forms,
  // i.e. forms with at least three fields, etc.
  // These are used to derive certain "user happiness" metrics.  For example, we
  // can compute the ratio (USER_DID_EDIT_AUTOFILLED_FIELD / USER_DID_AUTOFILL)
  // to see how often users have to correct autofilled data.
  enum UserHappinessMetric {
    // Loaded a page containing forms.
    FORMS_LOADED,
    // Submitted a fillable form -- i.e. one with at least three field values
    // that match the user's stored Autofill data -- and all matching fields
    // were autofilled.
    SUBMITTED_FILLABLE_FORM_AUTOFILLED_ALL,
    // Submitted a fillable form and some (but not all) matching fields were
    // autofilled.
    SUBMITTED_FILLABLE_FORM_AUTOFILLED_SOME,
    // Submitted a fillable form and no fields were autofilled.
    SUBMITTED_FILLABLE_FORM_AUTOFILLED_NONE,
    // Submitted a non-fillable form. This also counts entering new data into
    // a form with identified fields. Because we didn't have the data the user
    // wanted, from the user's perspective, the form was not autofillable.
    SUBMITTED_NON_FILLABLE_FORM,

    // User manually filled one of the form fields.
    USER_DID_TYPE,
    // We showed a popup containing Autofill suggestions.
    SUGGESTIONS_SHOWN,
    // Same as above, but only logged once per page load.
    SUGGESTIONS_SHOWN_ONCE,
    // User autofilled at least part of the form.
    USER_DID_AUTOFILL,
    // Same as above, but only logged once per page load.
    USER_DID_AUTOFILL_ONCE,
    // User edited a previously autofilled field.
    USER_DID_EDIT_AUTOFILLED_FIELD,
    // Same as above, but only logged once per page load.
    USER_DID_EDIT_AUTOFILLED_FIELD_ONCE,

    // User entered form data that appears to be a UPI Virtual Payment Address.
    USER_DID_ENTER_UPI_VPA,

    // A field was populated by autofill.
    FIELD_WAS_AUTOFILLED,

    NUM_USER_HAPPINESS_METRICS,
  };

  // Form Events for autofill.
  // These events are triggered separetly for address and credit card forms.
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
    // determine which of the following two will be fired.
    FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE,
    FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE,
    FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE,
    // A form was submitted. Depending on the user filling a local, server,
    // masked server card or no suggestion one of the following will be
    // triggered. Only one of the following four will be triggered per page
    // load.
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
    // logged. Depending on the user filling a local, server, masked server card
    // or no suggestion one of the following will be triggered, at most once per
    // page load.
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

    // The form was changed dynamically.
    FORM_EVENT_DID_SEE_DYNAMIC_FORM,
    // The form was changed dynamically and was fillable.
    FORM_EVENT_DID_SEE_FILLABLE_DYNAMIC_FORM,
    // There was a dynamic change of the form and it got re-filled
    // automatically.
    FORM_EVENT_DID_DYNAMIC_REFILL,
    // The form dynamically changed another time after the refill.
    FORM_EVENT_DYNAMIC_CHANGE_AFTER_REFILL,

    NUM_FORM_EVENTS,
  };

  // Indicates submitted card information.
  enum CardNumberStatus {
    EMPTY_CARD,
    WRONG_SIZE_CARD,
    FAIL_LUHN_CHECK_CARD,
    KNOWN_CARD,
    UNKNOWN_CARD
  };

  // Form Events for autofill with bank name available for display.
  enum BankNameDisplayedFormEvent {
    // A dropdown with suggestions was shown and at least one suggestion has a
    // bank name. Logged at most once per page load.
    FORM_EVENT_SUGGESTIONS_SHOWN_WITH_BANK_NAME_AVAILABLE_ONCE = 0,
    // A server suggestion was used to fill the form and at least one suggestion
    // has a bank name. Logged at most once per page load.
    FORM_EVENT_SERVER_SUGGESTION_FILLED_WITH_BANK_NAME_AVAILABLE_ONCE,
    BANK_NAME_NUM_FORM_EVENTS,
  };

  // Events related to the Unmask Credit Card Prompt.
  enum UnmaskPromptEvent {
    // The prompt was shown.
    UNMASK_PROMPT_SHOWN = 0,
    // The prompt was closed without attempting to unmask the card.
    UNMASK_PROMPT_CLOSED_NO_ATTEMPTS,
    // The prompt was closed without unmasking the card, but with at least
    // one attempt. The last failure was retriable.
    UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE,
    // The prompt was closed without unmasking the card, but with at least
    // one attempt. The last failure was non retriable.
    UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE,
    // Successfully unmasked the card in the first attempt.
    UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT,
    // Successfully unmasked the card after retriable failures.
    UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS,
    // Saved the card locally (masked card was upgraded to a full card).
    UNMASK_PROMPT_SAVED_CARD_LOCALLY,
    // User chose to opt in (checked the checkbox when it was empty).
    // Only logged if there was an attempt to unmask.
    UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_IN,
    // User did not opt in when they had the chance (left the checkbox
    // unchecked).  Only logged if there was an attempt to unmask.
    UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_IN,
    // User chose to opt out (unchecked the checkbox when it was check).
    // Only logged if there was an attempt to unmask.
    UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_OUT,
    // User did not opt out when they had a chance (left the checkbox checked).
    // Only logged if there was an attempt to unmask.
    UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_OUT,
    // The prompt was closed while chrome was unmasking the card (user pressed
    // verify and we were waiting for the server response).
    UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING,
    NUM_UNMASK_PROMPT_EVENTS,
  };

  // Possible results of Payments RPCs.
  enum PaymentsRpcResult {
    // Request succeeded.
    PAYMENTS_RESULT_SUCCESS = 0,
    // Request failed; try again.
    PAYMENTS_RESULT_TRY_AGAIN_FAILURE,
    // Request failed; don't try again.
    PAYMENTS_RESULT_PERMANENT_FAILURE,
    // Unable to connect to Payments servers.
    PAYMENTS_RESULT_NETWORK_ERROR,
    NUM_PAYMENTS_RESULTS,
  };

  // For measuring the network request time of various Wallet API calls. See
  // WalletClient::RequestType.
  enum WalletApiCallMetric {
    UNKNOWN_API_CALL,  // Catch all. Should never be used.
    ACCEPT_LEGAL_DOCUMENTS,
    AUTHENTICATE_INSTRUMENT,
    GET_FULL_WALLET,
    GET_WALLET_ITEMS,
    SAVE_TO_WALLET,
    NUM_WALLET_API_CALLS
  };

  // For measuring the frequency of errors while communicating with the Wallet
  // server.
  enum WalletErrorMetric {
    // Baseline metric: Issued a request to the Wallet server.
    WALLET_ERROR_BASELINE_ISSUED_REQUEST = 0,
    // A fatal error occured while communicating with the Wallet server. This
    // value has been deprecated.
    WALLET_FATAL_ERROR_DEPRECATED,
    // Received a malformed response from the Wallet server.
    WALLET_MALFORMED_RESPONSE,
    // A network error occured while communicating with the Wallet server.
    WALLET_NETWORK_ERROR,
    // The request was malformed.
    WALLET_BAD_REQUEST,
    // Risk deny, unsupported country, or account closed.
    WALLET_BUYER_ACCOUNT_ERROR,
    // Unknown server side error.
    WALLET_INTERNAL_ERROR,
    // API call had missing or invalid parameters.
    WALLET_INVALID_PARAMS,
    // Online Wallet is down.
    WALLET_SERVICE_UNAVAILABLE,
    // User needs make a cheaper transaction or not use Online Wallet.
    WALLET_SPENDING_LIMIT_EXCEEDED,
    // The server API version of the request is no longer supported.
    WALLET_UNSUPPORTED_API_VERSION,
    // Catch all error type.
    WALLET_UNKNOWN_ERROR,
    // The merchant has been blacklisted for Online Wallet due to some manner
    // of compliance violation.
    WALLET_UNSUPPORTED_MERCHANT,
    // Buyer Legal Address has a country which is unsupported by Wallet.
    WALLET_BUYER_LEGAL_ADDRESS_NOT_SUPPORTED,
    // Wallet's Know Your Customer(KYC) action is pending/failed for this user.
    WALLET_UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS,
    // Chrome version is unsupported or provided API key not allowed.
    WALLET_UNSUPPORTED_USER_AGENT_OR_API_KEY,
    NUM_WALLET_ERROR_METRICS
  };

  // For measuring the frequency of "required actions" returned by the Wallet
  // server.  This is similar to the autofill::wallet::RequiredAction enum;
  // but unlike that enum, the values in this one must remain constant over
  // time, so that the metrics can be consistently interpreted on the
  // server-side.
  enum WalletRequiredActionMetric {
    // Baseline metric: Issued a request to the Wallet server.
    WALLET_REQUIRED_ACTION_BASELINE_ISSUED_REQUEST = 0,
    // Values from the autofill::wallet::RequiredAction enum:
    UNKNOWN_REQUIRED_ACTION,  // Catch all type.
    GAIA_AUTH,
    PASSIVE_GAIA_AUTH,
    SETUP_WALLET,
    ACCEPT_TOS,
    UPDATE_EXPIRATION_DATE,
    UPGRADE_MIN_ADDRESS,
    CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS,
    VERIFY_CVV,
    INVALID_FORM_FIELD,
    REQUIRE_PHONE_NUMBER,
    NUM_WALLET_REQUIRED_ACTIONS
  };

  // For mesuring how wallet addresses are converted to local profiles.
  enum WalletAddressConversionType : int {
    // The converted wallet address was merged into an existing local profile.
    CONVERTED_ADDRESS_MERGED,
    // The converted wallet address was added as a new local profile.
    CONVERTED_ADDRESS_ADDED,
    NUM_CONVERTED_ADDRESS_CONVERSION_TYPES
  };

  // Utility to log URL keyed form interaction events.
  class FormInteractionsUkmLogger {
   public:
    explicit FormInteractionsUkmLogger(ukm::UkmRecorder* ukm_recorder);

    bool has_pinned_timestamp() const { return !pinned_timestamp_.is_null(); }
    void set_pinned_timestamp(base::TimeTicks t) { pinned_timestamp_ = t; }

    const GURL& url() const { return url_; }

    // Initializes this logger with a valid url and source_id.
    // Unless forms is parsed no autofill UKM can be recorded.
    void OnFormsParsed(const GURL& url, const ukm::SourceId source_id);
    void LogInteractedWithForm(bool is_for_credit_card,
                               size_t local_record_type_count,
                               size_t server_record_type_count,
                               FormSignature form_signature);
    void LogSuggestionsShown(const FormStructure& form,
                             const AutofillField& field,
                             const base::TimeTicks& form_parsed_timestamp);
    void LogSelectedMaskedServerCard(
        const base::TimeTicks& form_parsed_timestamp);
    void LogDidFillSuggestion(int record_type,
                              bool is_for_credit_card,
                              const FormStructure& form,
                              const AutofillField& field);
    void LogTextFieldDidChange(const FormStructure& form,
                               const AutofillField& field);
    void LogFieldFillStatus(const FormStructure& form,
                            const AutofillField& field,
                            QualityMetricType metric_type);
    void LogFieldType(const base::TimeTicks& form_parsed_timestamp,
                      FormSignature form_signature,
                      FieldSignature field_signature,
                      QualityMetricPredictionSource prediction_source,
                      QualityMetricType metric_type,
                      ServerFieldType predicted_type,
                      ServerFieldType actual_type);
    void LogFormSubmitted(bool is_for_credit_card,
                          const std::set<FormType>& form_types,
                          AutofillFormSubmittedState state,
                          const base::TimeTicks& form_parsed_timestamp,
                          FormSignature form_signature);

    // Log whether the autofill decided to skip or to fill each
    // hidden/representational field.
    void LogHiddenRepresentationalFieldSkipDecision(const FormStructure& form,
                                                    const AutofillField& field,
                                                    bool is_skipped);

    // Log the fields for which the autofill decided to rationalize the server
    // type predictions due to repetition of the type.
    void LogRepeatedServerTypePredictionRationalized(
        const FormSignature form_signature,
        const AutofillField& field,
        ServerFieldType old_type);

   private:
    bool CanLog() const;
    int64_t MillisecondsSinceFormParsed(
        const base::TimeTicks& form_parsed_timestamp) const;

    ukm::UkmRecorder* ukm_recorder_;  // Weak reference.
    ukm::SourceId source_id_ = -1;
    GURL url_;
    base::TimeTicks pinned_timestamp_;
  };

  // Utility class to pin the timestamp used by the FormInteractionsUkmLogger
  // while an instance of this class is in scope. Pinned timestamps cannot be
  // nested.
  class UkmTimestampPin {
   public:
    UkmTimestampPin(FormInteractionsUkmLogger* logger);
    ~UkmTimestampPin();

   private:
    FormInteractionsUkmLogger* const logger_;
    DISALLOW_IMPLICIT_CONSTRUCTORS(UkmTimestampPin);
  };

  static void LogSubmittedCardStateMetric(SubmittedCardStateMetric metric);

  // If a credit card that matches a server card (unmasked or not) was submitted
  // on a form, logs whether the submitted card's expiration date matched the
  // server card's known expiration date.
  static void LogSubmittedServerCardExpirationStatusMetric(
      SubmittedServerCardExpirationStatusMetric metric);

  // When credit card save is not offered (either at all on mobile or by simply
  // not showing the bubble on desktop), logs the occurrence.
  static void LogCreditCardSaveNotOfferedDueToMaxStrikesMetric(
      SaveTypeMetric metric);

  // When credit card upload is disallowed for a particular network, logs which
  // network was blocked.
  static void LogUploadDisallowedForNetworkMetric(const std::string& network);

  // When credit card upload is offered, logs whether the card being offered is
  // already a local card on the device or not.
  static void LogUploadOfferedCardOriginMetric(
      UploadOfferedCardOriginMetric metric);

  // When credit card upload is accepted, logs whether the card being accepted
  // is already a local card on the device or not.
  static void LogUploadAcceptedCardOriginMetric(
      UploadAcceptedCardOriginMetric metric);

  // When a cardholder name fix flow is shown during credit card upload, logs
  // whether the cardholder name was prefilled or not.
  static void LogSaveCardCardholderNamePrefilled(bool prefilled);

  // When a cardholder name fix flow is shown during credit card upload and the
  // user accepts upload, logs whether the final cardholder name was changed
  // from its prefilled value or not.
  static void LogSaveCardCardholderNameWasEdited(bool edited);

  // Logs the PaymentsCustomerData billing ID status at the time of use.
  static void LogPaymentsCustomerDataBillingIdStatus(BillingIdStatus status);

  // |upload_decision_metrics| is a bitmask of |CardUploadDecisionMetric|.
  static void LogCardUploadDecisionMetrics(int upload_decision_metrics);
  static void LogCreditCardInfoBarMetric(
      InfoBarMetric metric,
      bool is_uploading,
      int previous_save_credit_card_prompt_user_decision);
  static void LogCreditCardFillingInfoBarMetric(InfoBarMetric metric);
  static void LogSaveCardPromptMetric(
      SaveCardPromptMetric metric,
      bool is_uploading,
      bool is_reshow,
      bool is_requesting_cardholder_name,
      int previous_save_credit_card_prompt_user_decision,
      security_state::SecurityLevel security_level);
  static void LogSaveCardPromptMetricBySecurityLevel(
      SaveCardPromptMetric metric,
      bool is_uploading,
      security_state::SecurityLevel security_level);
  static void LogManageCardsPromptMetric(ManageCardsPromptMetric metric,
                                         bool is_uploading);
  static void LogScanCreditCardPromptMetric(ScanCreditCardPromptMetric metric);
  static void LogLocalCardMigrationBubbleOfferMetric(
      LocalCardMigrationBubbleOfferMetric metric,
      bool is_reshow);
  static void LogLocalCardMigrationBubbleUserInteractionMetric(
      LocalCardMigrationBubbleUserInteractionMetric metric,
      bool is_reshow);
  static void LogLocalCardMigrationDialogOfferMetric(
      LocalCardMigrationDialogOfferMetric metric);
  static void LogLocalCardMigrationDialogUserInteractionMetric(
      const base::TimeDelta& duration,
      const int selected,
      const int total,
      LocalCardMigrationDialogUserInteractionMetric metric);
  static void LogLocalCardMigrationPromptMetric(
      LocalCardMigrationOrigin local_card_migration_origin,
      LocalCardMigrationPromptMetric metric);

  // Should be called when credit card scan is finished. |duration| should be
  // the time elapsed between launching the credit card scanner and getting back
  // the result. |completed| should be true if a credit card was scanned, false
  // if the scan was cancelled.
  static void LogScanCreditCardCompleted(const base::TimeDelta& duration,
                                         bool completed);

  static void LogSaveCardWithFirstAndLastNameOffered(bool is_local);
  static void LogSaveCardWithFirstAndLastNameComplete(bool is_local);

  static void LogDeveloperEngagementMetric(DeveloperEngagementMetric metric);

  static void LogHeuristicPredictionQualityMetrics(
      FormInteractionsUkmLogger* form_interactions_ukm_logger,
      const FormStructure& form,
      const AutofillField& field,
      QualityMetricType metric_type);
  static void LogServerPredictionQualityMetrics(
      FormInteractionsUkmLogger* form_interactions_ukm_logger,
      const FormStructure& form,
      const AutofillField& field,
      QualityMetricType metric_type);
  static void LogOverallPredictionQualityMetrics(
      FormInteractionsUkmLogger* form_interactions_ukm_logger,
      const FormStructure& form,
      const AutofillField& field,
      QualityMetricType metric_type);

  static void LogServerQueryMetric(ServerQueryMetric metric);

  static void LogUserHappinessMetric(
      UserHappinessMetric metric,
      FieldTypeGroup field_type_group,
      security_state::SecurityLevel security_level);

  static void LogUserHappinessMetric(
      UserHappinessMetric metric,
      const std::set<FormType>& form_types,
      security_state::SecurityLevel security_level);

  static void LogUserHappinessBySecurityLevel(
      UserHappinessMetric metric,
      FormType form_type,
      security_state::SecurityLevel security_level);

  // Logs |event| to the unmask prompt events histogram.
  static void LogUnmaskPromptEvent(UnmaskPromptEvent event);

  // Logs the time elapsed between the unmask prompt being shown and it
  // being closed.
  static void LogUnmaskPromptEventDuration(const base::TimeDelta& duration,
                                           UnmaskPromptEvent close_event);

  // Logs the time elapsed between the user clicking Verify and
  // hitting cancel when abandoning a pending unmasking operation
  // (aka GetRealPan).
  static void LogTimeBeforeAbandonUnmasking(const base::TimeDelta& duration);

  // Logs |result| to the get real pan result histogram.
  static void LogRealPanResult(AutofillClient::PaymentsRpcResult result);

  // Logs |result| to duration of the GetRealPan RPC.
  static void LogRealPanDuration(const base::TimeDelta& duration,
                                 AutofillClient::PaymentsRpcResult result);

  // Logs |result| to the get real pan result histogram.
  static void LogUnmaskingDuration(const base::TimeDelta& duration,
                                   AutofillClient::PaymentsRpcResult result);

  // This should be called when a form that has been Autofilled is submitted.
  // |duration| should be the time elapsed between form load and submission.
  static void LogFormFillDurationFromLoadWithAutofill(
      const base::TimeDelta& duration);

  // This should be called when a fillable form that has not been Autofilled is
  // submitted.  |duration| should be the time elapsed between form load and
  // submission.
  static void LogFormFillDurationFromLoadWithoutAutofill(
      const base::TimeDelta& duration);

  // This should be called when a form is submitted. |duration| should be the
  // time elapsed between the initial form interaction and submission. This
  // metric is sliced by |form_type| and |used_autofill|.
  static void LogFormFillDurationFromInteraction(
      const std::set<FormType>& form_types,
      bool used_autofill,
      const base::TimeDelta& duration);

  static void LogFormFillDuration(const std::string& metric,
                                  const base::TimeDelta& duration);

  // This should be called each time a page containing forms is loaded.
  static void LogIsAutofillEnabledAtPageLoad(bool enabled);

  // This should be called each time a new chrome profile is launched.
  static void LogIsAutofillEnabledAtStartup(bool enabled);

  // Records the number of stored address profiles. This is be called each time
  // a new chrome profile is launched.
  static void LogStoredProfileCount(size_t num_profiles);

  // Records the number of stored address profiles which have not been used in
  // a long time. This is be called each time a new chrome profile is launched.
  static void LogStoredProfileDisusedCount(size_t num_profiles);

  // Records the number of days since an address profile was last used. This is
  // called once per address profile each time a new chrome profile is launched.
  static void LogStoredProfileDaysSinceLastUse(size_t days);

  // Logs various metrics about the local and server cards associated with a
  // profile. This should be called each time a new chrome profile is launched.
  static void LogStoredCreditCardMetrics(
      const std::vector<std::unique_ptr<CreditCard>>& local_cards,
      const std::vector<std::unique_ptr<CreditCard>>& server_cards,
      base::TimeDelta disused_data_threshold);

  // Log the number of autofill credit card suggestions suppressed because they
  // have not been used for a long time and are expired. Note that these cards
  // are only suppressed when the user has not typed any data into the field
  // from which autofill is triggered. Credit cards matching something the user
  // has types are always offered, regardless of how recently they have been
  // used.
  static void LogNumberOfCreditCardsSuppressedForDisuse(size_t num_cards);

  // Log the number of autofill credit card deleted during major version upgrade
  // because they have not been used for a long time and are expired.
  static void LogNumberOfCreditCardsDeletedForDisuse(size_t num_cards);

  // Log the number of profiles available when an autofillable form is
  // submitted.
  static void LogNumberOfProfilesAtAutofillableFormSubmission(
      size_t num_profiles);

  // Log whether user modified an address profile shortly before submitting
  // credit card form.
  static void LogHasModifiedProfileOnCreditCardFormSubmission(
      bool has_modified_profile);

  // Log the number of autofill address suggestions suppressed because they have
  // not been used for a long time. Note that these addresses are only
  // suppressed when the user has not typed any data into the field from which
  // autofill is triggered. Addresses matching something the user has types are
  // always offered, regardless of how recently they have been used.
  static void LogNumberOfAddressesSuppressedForDisuse(size_t num_profiles);

  // Log the number of unverified autofill addresses deleted because they have
  // not been used for a long time, and are not used as billing addresses of
  // valid credit cards. Note the deletion only happens once per major version
  // upgrade.
  static void LogNumberOfAddressesDeletedForDisuse(size_t num_profiles);

  // Log the number of Autofill address suggestions presented to the user when
  // filling a form.
  static void LogAddressSuggestionsCount(size_t num_suggestions);

  // Log the index of the selected Autofill suggestion in the popup.
  static void LogAutofillSuggestionAcceptedIndex(int index);

  // Log the index of the selected Autocomplete suggestion in the popup.
  static void LogAutocompleteSuggestionAcceptedIndex(int index);

  // Log how many autofilled fields in a given form were edited before the
  // submission or when the user unfocused the form (depending on
  // |observed_submission|).
  static void LogNumberOfEditedAutofilledFields(
      size_t num_edited_autofilled_fields,
      bool observed_submission);

  // This should be called each time a server response is parsed for a form.
  static void LogServerResponseHasDataForForm(bool has_data);

  // This should be called at each form submission to indicate what profile
  // action happened.
  static void LogProfileActionOnFormSubmitted(AutofillProfileAction action);

  // This should be called at each form submission to indicate the autofilled
  // state of the form.
  static void LogAutofillFormSubmittedState(
      AutofillFormSubmittedState state,
      bool is_for_credit_card,
      const std::set<FormType>& form_types,
      const base::TimeTicks& form_parsed_timestamp,
      FormSignature form_signature,
      FormInteractionsUkmLogger* form_interactions_ukm_logger);

  // This should be called when determining the heuristic types for a form's
  // fields.
  static void LogDetermineHeuristicTypesTiming(const base::TimeDelta& duration);

  // This should be called when parsing each form.
  static void LogParseFormTiming(const base::TimeDelta& duration);

  // Log how many profiles were considered for the deduplication process.
  static void LogNumberOfProfilesConsideredForDedupe(size_t num_considered);

  // Log how many profiles were removed as part of the deduplication process.
  static void LogNumberOfProfilesRemovedDuringDedupe(size_t num_removed);

  // Log whether the Autofill query on a credit card form is made in a secure
  // context.
  static void LogIsQueriedCreditCardFormSecure(bool is_secure);

  // Log how the converted wallet address was added to the local autofill
  // profiles.
  static void LogWalletAddressConversionType(WalletAddressConversionType type);

  // This should be called when the user selects the Form-Not-Secure warning
  // suggestion to show an explanation of the warning.
  static void LogShowedHttpNotSecureExplanation();

  // Logs if an autocomplete query was created for a field.
  static void LogAutocompleteQuery(bool created);

  // Logs if there is any suggestions for an autocomplete query.
  static void LogAutocompleteSuggestions(bool has_suggestions);

  // Returns the UMA metric used to track whether or not an upload was sent
  // after being triggered by |submission_source|. This is exposed for testing.
  static const char* SubmissionSourceToUploadEventMetric(
      SubmissionSource submission_source);

  // Logs whether or not an upload |was_sent| after being triggered by a
  // |submission_source| event.
  static void LogUploadEvent(SubmissionSource submission_source, bool was_sent);

  // Logs the card upload decisions ukm for the specified |url|.
  // |upload_decision_metrics| is a bitmask of |CardUploadDecisionMetric|.
  static void LogCardUploadDecisionsUkm(ukm::UkmRecorder* ukm_recorder,
                                        ukm::SourceId source_id,
                                        const GURL& url,
                                        int upload_decision_metrics);

  // Logs the developer engagement ukm for the specified |url| and autofill
  // fields in the form structure. |developer_engagement_metrics| is a bitmask
  // of |AutofillMetrics::DeveloperEngagementMetric|. |is_for_credit_card| is
  // true if the form is a credit card form. |form_types| is set of
  // FormType recorded for the page. This will be stored as a bit vector
  // in UKM.
  static void LogDeveloperEngagementUkm(ukm::UkmRecorder* ukm_recorder,
                                        ukm::SourceId source_id,
                                        const GURL& url,
                                        bool is_for_credit_card,
                                        std::set<FormType> form_types,
                                        int developer_engagement_metrics,
                                        FormSignature form_signature);

  // Log the number of hidden or presentational 'select' fields that were
  // autofilled to support synthetic fields.
  static void LogHiddenOrPresentationalSelectFieldsFilled();

  // Logs the the |ukm_entry_name| with the specified |url| and the specified
  // |metrics|. Returns whether the ukm was sucessfully logged.
  static bool LogUkm(ukm::UkmRecorder* ukm_recorder,
                     const GURL& url,
                     const std::string& ukm_entry_name,
                     const std::vector<std::pair<const char*, int>>& metrics);

  // Converts form type to bit vector to store in UKM.
  static int64_t FormTypesToBitVector(const std::set<FormType>& form_types);

  // Utility to log autofill form events in the relevant histograms depending on
  // the presence of server and/or local data.
  class FormEventLogger {
   public:
    FormEventLogger(bool is_for_credit_card,
                    bool is_in_main_frame,
                    FormInteractionsUkmLogger* form_interactions_ukm_logger);

    inline void set_server_record_type_count(size_t server_record_type_count) {
      server_record_type_count_ = server_record_type_count;
    }

    inline void set_local_record_type_count(size_t local_record_type_count) {
      local_record_type_count_ = local_record_type_count;
    }

    inline void set_is_context_secure(bool is_context_secure) {
      is_context_secure_ = is_context_secure;
    }

    void OnDidInteractWithAutofillableForm(FormSignature form_signature);

    void OnDidPollSuggestions(const FormFieldData& field);

    void OnDidShowSuggestions(const FormStructure& form,
                              const AutofillField& field,
                              const base::TimeTicks& form_parsed_timestamp);

    void OnDidSelectMaskedServerCardSuggestion(
        const base::TimeTicks& form_parsed_timestamp);

    // In case of masked cards, caller must make sure this gets called before
    // the card is upgraded to a full card.
    void OnDidFillSuggestion(const CreditCard& credit_card,
                             const FormStructure& form,
                             const AutofillField& field);

    void OnDidFillSuggestion(const AutofillProfile& profile,
                             const FormStructure& form,
                             const AutofillField& field);

    void OnWillSubmitForm();

    void OnFormSubmitted(bool force_logging,
                         const CardNumberStatus card_number_status);

    void SetBankNameAvailable();

    void OnDidSeeDynamicForm();

    void OnDidSeeFillableDynamicForm();

    void OnDidRefill();

    void OnSubsequentRefillAttempt();

   private:
    void Log(FormEvent event) const;
    void Log(BankNameDisplayedFormEvent event) const;

    bool is_for_credit_card_;
    bool is_in_main_frame_;
    size_t server_record_type_count_;
    size_t local_record_type_count_;
    bool is_context_secure_;
    bool has_logged_interacted_;
    bool has_logged_suggestions_shown_;
    bool has_logged_masked_server_card_suggestion_selected_;
    bool has_logged_suggestion_filled_;
    bool has_logged_will_submit_;
    bool has_logged_submitted_;
    bool has_logged_bank_name_available_;
    bool logged_suggestion_filled_was_server_data_;
    bool logged_suggestion_filled_was_masked_server_card_;

    // The last field that was polled for suggestions.
    FormFieldData last_polled_field_;

    FormInteractionsUkmLogger*
        form_interactions_ukm_logger_;  // Weak reference.
  };

 private:
  static const int kNumCardUploadDecisionMetrics = 14;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AutofillMetrics);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_
