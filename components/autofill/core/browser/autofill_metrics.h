// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_

#include <stddef.h>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/form_events.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/signatures.h"
#include "components/security_state/core/security_state.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace autofill {

class AutofillField;
class CreditCard;
struct AutofillOfferData;

// A given maximum is enforced to minimize the number of buckets generated.
extern const int kMaxBucketsCount;

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
    // A pair of dropdowns for the user to select expiration date was surfaced
    // in the offer-to-save dialog.
    USER_REQUESTED_TO_PROVIDE_EXPIRATION_DATE = 1 << 14,
    // All the required conditions were satisfied even though the form is
    // unfocused after the user entered information into it.
    UPLOAD_OFFERED_FROM_NON_FOCUSABLE_FIELD = 1 << 15,
    // The card does not satisfy any of the ranges of supported BIN ranges.
    UPLOAD_NOT_OFFERED_UNSUPPORTED_BIN_RANGE = 1 << 16,
    // All the required conditions were satisfied even though the form is
    // dynamic changed.
    UPLOAD_OFFERED_FROM_DYNAMIC_CHANGE_FORM = 1 << 17,
    // The legal message was invalid.
    UPLOAD_NOT_OFFERED_INVALID_LEGAL_MESSAGE = 1 << 18,
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
    INFOBAR_ACCEPTED,  // The user explicitly accepted the infobar.
    INFOBAR_DENIED,    // The user explicitly denied the infobar.
    INFOBAR_IGNORED,   // The user completely ignored the infobar (logged on
    // tab close).
    INFOBAR_NOT_SHOWN_INVALID_LEGAL_MESSAGE,  // We didn't show the infobar
    // because the provided legal
    // message was invalid.
    NUM_INFO_BAR_METRICS,
  };

  // Autocomplete Events.
  // These events are not based on forms nor submissions, but depend on the
  // the usage of the Autocomplete feature.
  enum AutocompleteEvent {
    // A dropdown with Autocomplete suggestions was shown.
    AUTOCOMPLETE_SUGGESTIONS_SHOWN = 0,

    // An Autocomplete suggestion was selected.
    AUTOCOMPLETE_SUGGESTION_SELECTED,

    NUM_AUTOCOMPLETE_EVENTS
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
  // the same last four digits (if masked).
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

  // Represents requesting expiration date reason.
  enum class SaveCardRequestExpirationDateReasonMetric {
    // Submitted card has empty month.
    kMonthMissingOnly,
    // Submitted card has empty year.
    kYearMissingOnly,
    // Submitted card has both empty month and year.
    kMonthAndYearMissing,
    // Submitted card has expired expiration date.
    kExpirationDatePresentButExpired,
    kMaxValue = kExpirationDatePresentButExpired,
  };

  // Metrics to track event when the save card prompt is offered.
  enum SaveCardPromptOfferMetric {
    // The prompt is actually shown.
    SAVE_CARD_PROMPT_SHOWN,
    // The prompt is not shown because the prompt has been declined by the user
    // too many times.
    SAVE_CARD_PROMPT_NOT_SHOWN_MAX_STRIKES_REACHED,
    NUM_SAVE_CARD_PROMPT_OFFER_METRICS,
  };

  enum SaveCardPromptResultMetric {
    // The user explicitly accepted the prompt by clicking the ok button.
    SAVE_CARD_PROMPT_ACCEPTED,
    // The user explicitly cancelled the prompt by clicking the cancel button.
    SAVE_CARD_PROMPT_CANCELLED,
    // The user explicitly closed the prompt with the close button or ESC.
    SAVE_CARD_PROMPT_CLOSED,
    // The user did not interact with the prompt.
    SAVE_CARD_PROMPT_NOT_INTERACTED,
    // The prompt lost focus and was deactivated.
    SAVE_CARD_PROMPT_LOST_FOCUS,
    // The reason why the prompt is closed is not clear. Possible reason is the
    // logging function is invoked before the closed reason is correctly set.
    SAVE_CARD_PROMPT_RESULT_UNKNOWN,
    NUM_SAVE_CARD_PROMPT_RESULT_METRICS,
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
    SAVE_CARD_PROMPT_SHOWN_DEPRECATED,
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

    // The save card bubble was not shown due to the card having too many
    // offer-to-save strikes, but the omnibox icon was still displayed.
    SAVE_CARD_ICON_SHOWN_WITHOUT_PROMPT,

    NUM_SAVE_CARD_PROMPT_METRICS,
  };

  enum CreditCardUploadFeedbackMetric {
    // The loading indicator animation which indicates uploading is in progress
    // is successfully shown.
    CREDIT_CARD_UPLOAD_FEEDBACK_LOADING_ANIMATION_SHOWN,
    // The credit card icon with the saving failure badge is shown.
    CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_ICON_SHOWN,
    // The failure icon is clicked and the save card failure bubble is shown.
    CREDIT_CARD_UPLOAD_FEEDBACK_FAILURE_BUBBLE_SHOWN,
    NUM_CREDIT_CARD_UPLOAD_FEEDBACK_METRICS,
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

  // Metrics to record the decision on whether to offer local card migration.
  enum class LocalCardMigrationDecisionMetric {
    // All the required conditions are satisfied and main prompt is shown.
    OFFERED = 0,
    // Migration not offered because user uses new card.
    NOT_OFFERED_USE_NEW_CARD = 1,
    // Migration not offered because failed migration prerequisites.
    NOT_OFFERED_FAILED_PREREQUISITES = 2,
    // The Autofill StrikeDatabase decided not to allow offering migration
    // because max strike count was reached.
    NOT_OFFERED_REACHED_MAX_STRIKE_COUNT = 3,
    // Migration not offered because no migratable cards.
    NOT_OFFERED_NO_MIGRATABLE_CARDS = 4,
    // Met the migration requirements but the request to Payments for upload
    // details failed.
    NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED = 5,
    // Abandoned the migration because no supported local cards were left after
    // filtering out unsupported cards.
    NOT_OFFERED_NO_SUPPORTED_CARDS = 6,
    // User used a local card and they only have a single migratable local card
    // on file, we will offer Upstream instead.
    NOT_OFFERED_SINGLE_LOCAL_CARD = 7,
    // User used an unsupported local card, we will abort the migration.
    NOT_OFFERED_USE_UNSUPPORTED_LOCAL_CARD = 8,
    // Legal message was invalid, we will abort the migration.
    NOT_OFFERED_INVALID_LEGAL_MESSAGE = 9,
    kMaxValue = NOT_OFFERED_INVALID_LEGAL_MESSAGE,
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
  // TODO(crbug.com/1070799): Remove this enum once the old logging is cleaned
  // up.
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

  // Metrics to track user action result of the bubble when the bubble is
  // closed.
  enum LocalCardMigrationBubbleResultMetric {
    // The user explicitly accepted the offer.
    LOCAL_CARD_MIGRATION_BUBBLE_ACCEPTED = 0,
    // The user explicitly closed the bubble with the close button or ESC.
    LOCAL_CARD_MIGRATION_BUBBLE_CLOSED = 1,
    // The user did not interact with the bubble.
    LOCAL_CARD_MIGRATION_BUBBLE_NOT_INTERACTED = 2,
    // The bubble lost its focus and was deactivated.
    LOCAL_CARD_MIGRATION_BUBBLE_LOST_FOCUS = 3,
    // The reason why the prompt is closed is not clear. Possible reason is the
    // logging function is invoked before the closed reason is correctly set.
    LOCAL_CARD_MIGRATION_BUBBLE_RESULT_UNKNOWN = 4,
    NUM_LOCAL_CARD_MIGRATION_BUBBLE_RESULT_METRICS,
  };

  // Metrics to track events when local card migration dialog is offered.
  enum LocalCardMigrationDialogOfferMetric {
    // The dialog is shown to the user.
    LOCAL_CARD_MIGRATION_DIALOG_SHOWN = 0,
    // The dialog is not shown due to legal message being invalid.
    LOCAL_CARD_MIGRATION_DIALOG_NOT_SHOWN_INVALID_LEGAL_MESSAGE = 1,
    // The dialog is shown when migration feedback is available.
    LOCAL_CARD_MIGRATION_DIALOG_FEEDBACK_SHOWN = 2,
    // The dialog is shown when migration fails due to server error.
    LOCAL_CARD_MIGRATION_DIALOG_FEEDBACK_SERVER_ERROR_SHOWN = 3,
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
    // The user clicks the view card button after successfully migrated cards.
    LOCAL_CARD_MIGRATION_DIALOG_CLOSED_VIEW_CARDS_BUTTON_CLICKED = 3,
    // The user clicks the done button to close dialog after migration.
    LOCAL_CARD_MIGRATION_DIALOG_CLOSED_DONE_BUTTON_CLICKED = 4,
    // The user clicks the trash icon to delete invalid card.
    LOCAL_CARD_MIGRATION_DIALOG_DELETE_CARD_ICON_CLICKED = 5,
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

  // Cardholder name fix flow prompt metrics.
  enum CardholderNameFixFlowPromptEvent {
    // The prompt was shown.
    CARDHOLDER_NAME_FIX_FLOW_PROMPT_SHOWN = 0,
    // The prompt was accepted by user.
    CARDHOLDER_NAME_FIX_FLOW_PROMPT_ACCEPTED,
    // The prompt was dismissed by user.
    CARDHOLDER_NAME_FIX_FLOW_PROMPT_DISMISSED,
    // The prompt was closed without user interaction.
    CARDHOLDER_NAME_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION,
    NUM_CARDHOLDER_NAME_FIXFLOW_PROMPT_EVENTS,
  };

  // Expiration date fix flow prompt metrics.
  enum class ExpirationDateFixFlowPromptEvent {
    // The prompt was accepted by user.
    EXPIRATION_DATE_FIX_FLOW_PROMPT_ACCEPTED = 0,
    // The prompt was dismissed by user.
    EXPIRATION_DATE_FIX_FLOW_PROMPT_DISMISSED,
    // The prompt was closed without user interaction.
    EXPIRATION_DATE_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION,
    kMaxValue = EXPIRATION_DATE_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION,
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

  // Events related to user-perceived latency due to GetDetailsForGetRealPan
  // call.
  enum class PreflightCallEvent {
    // Returned before card chosen.
    kPreflightCallReturnedBeforeCardChosen = 0,
    // Did not return before card was chosen. When opted-in, this means
    // the UI had to wait for the call to return. When opted-out, this means we
    // did not offer to opt-in.
    kCardChosenBeforePreflightCallReturned = 1,
    // Preflight call was irrelevant; skipped waiting.
    kDidNotChooseMaskedCard = 2,
    kMaxValue = kDidNotChooseMaskedCard,
  };

  // Metric for tracking which authentication method was used for a user with
  // FIDO authentication enabled.
  enum class CardUnmaskTypeDecisionMetric {
    // Only WebAuthn prompt was shown.
    kFidoOnly = 0,
    // CVC authentication was required in addition to WebAuthn.
    kCvcThenFido = 1,
    kMaxValue = kCvcThenFido,
  };

  // Possible scenarios where a WebAuthn prompt may show.
  enum class WebauthnFlowEvent {
    // WebAuthn is immediately prompted for unmasking.
    kImmediateAuthentication = 0,
    // WebAuthn is prompted after a CVC check.
    kAuthenticationAfterCvc = 1,
    // WebAuthn is prompted after being offered to opt-in from a checkout flow.
    kCheckoutOptIn = 2,
    // WebAuthn is prompted after being offered to opt-in from the settings
    // page.
    kSettingsPageOptIn = 3,
    kMaxValue = kSettingsPageOptIn,
  };

  // The result of a WebAuthn user-verification prompt.
  enum class WebauthnResultMetric {
    // User-verification succeeded.
    kSuccess = 0,
    // Other checks failed (e.g. invalid domain, algorithm unsupported, etc.)
    kOtherError = 1,
    // User either failed verification or cancelled.
    kNotAllowedError = 2,
    kMaxValue = kNotAllowedError,
  };

  // The user decision for the WebAuthn opt-in promo.
  enum class WebauthnOptInPromoUserDecisionMetric {
    // User accepted promo.
    kAccepted = 0,
    // User immediately declined promo.
    kDeclinedImmediately = 1,
    // Once user accepts the dialog, a round-trip call to Payments is sent,
    // which is required for user authentication. The user has the option to
    // cancel the dialog before the round-trip call is returned.
    kDeclinedAfterAccepting = 2,
    kMaxValue = kDeclinedAfterAccepting,
  };

  // The parameters with which opt change was called.
  enum class WebauthnOptInParameters {
    // Call made to fetch a challenge.
    kFetchingChallenge = 0,
    // Call made with signature of creation challenge.
    kWithCreationChallenge = 1,
    // Call made with signature of request challenge.
    kWithRequestChallenge = 2,
    kMaxValue = kWithRequestChallenge,
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
    // The merchant has been blocked for Online Wallet due to some manner of
    // compliance violation.
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

  // For measuring how wallet addresses are converted to local profiles.
  enum WalletAddressConversionType : int {
    // The converted wallet address was merged into an existing local profile.
    CONVERTED_ADDRESS_MERGED,
    // The converted wallet address was added as a new local profile.
    CONVERTED_ADDRESS_ADDED,
    NUM_CONVERTED_ADDRESS_CONVERSION_TYPES
  };

  // To record whether the upload event was sent.
  enum class UploadEventStatus { kNotSent, kSent, kMaxValue = kSent };

  // Log all the scenarios that contribute to the decision of whether card
  // upload is enabled or not.
  enum class CardUploadEnabledMetric {
    SYNC_SERVICE_NULL = 0,
    SYNC_SERVICE_PERSISTENT_AUTH_ERROR = 1,
    SYNC_SERVICE_MISSING_AUTOFILL_WALLET_DATA_ACTIVE_TYPE = 2,
    SYNC_SERVICE_MISSING_AUTOFILL_PROFILE_ACTIVE_TYPE = 3,
    // Deprecated: ACCOUNT_WALLET_STORAGE_UPLOAD_DISABLED = 4,
    USING_SECONDARY_SYNC_PASSPHRASE = 5,
    LOCAL_SYNC_ENABLED = 6,
    PAYMENTS_INTEGRATION_DISABLED = 7,
    EMAIL_EMPTY = 8,
    EMAIL_DOMAIN_NOT_SUPPORTED = 9,
    AUTOFILL_UPSTREAM_DISABLED = 10,
    CARD_UPLOAD_ENABLED = 11,
    kMaxValue = CARD_UPLOAD_ENABLED,
  };

  // Enumerates the status of the  different requirements to successfully import
  // an address profile from a form submission.
  enum class AddressProfileImportRequirementMetric {
    // The form must contain either no or only a single unique email address.
    EMAIL_ADDRESS_UNIQUE_REQUIREMENT_FULFILLED = 0,
    EMAIL_ADDRESS_UNIQUE_REQUIREMENT_VIOLATED = 1,
    // The form is not allowed to contain invalid field types.
    NO_INVALID_FIELD_TYPES_REQUIREMENT_FULFILLED = 2,
    NO_INVALID_FIELD_TYPES_REQUIREMENT_VIOLATED = 3,
    // If required by |CountryData|, the form must contain a city entry.
    CITY_REQUIREMENT_FULFILLED = 4,
    CITY_REQUIREMENT_VIOLATED = 5,
    // If required by |CountryData|, the form must contain a state entry.
    STATE_REQUIREMENT_FULFILLED = 6,
    STATE_REQUIREMENT_VIOLATED = 7,
    // If required by |CountryData|, the form must contain a ZIP entry.
    ZIP_REQUIREMENT_FULFILLED = 8,
    ZIP_REQUIREMENT_VIOLATED = 9,
    // If present, the email address must be valid.
    EMAIL_VALID_REQUIREMENT_FULFILLED = 10,
    EMAIL_VALID_REQUIREMENT_VIOLATED = 11,
    // If present, the country must be valid.
    COUNTRY_VALID_REQUIREMENT_FULFILLED = 12,
    COUNTRY_VALID_REQUIREMENT_VIOLATED = 13,
    // If present, the state must be valid (if verifiable).
    STATE_VALID_REQUIREMENT_FULFILLED = 14,
    STATE_VALID_REQUIREMENT_VIOLATED = 15,
    // If present, the ZIP must be valid (if verifiable).
    ZIP_VALID_REQUIREMENT_FULFILLED = 16,
    ZIP_VALID_REQUIREMENT_VIOLATED = 17,
    // If present, the phone number must be valid (if verifiable).
    PHONE_VALID_REQUIREMENT_FULFILLED = 18,
    PHONE_VALID_REQUIREMENT_VIOLATED = 19,
    // Indicates the overall status of the import requirements check.
    OVERALL_REQUIREMENT_FULFILLED = 20,
    OVERALL_REQUIREMENT_VIOLATED = 21,
    // If required by |CountryData|, the form must contain a line1 entry.
    LINE1_REQUIREMENT_FULFILLED = 22,
    LINE1_REQUIREMENT_VIOLATED = 23,
    // If required by |CountryData|, the form must contain a either a zip or a
    // state entry.
    ZIP_OR_STATE_REQUIREMENT_FULFILLED = 24,
    ZIP_OR_STATE_REQUIREMENT_VIOLATED = 25,
    // Must be set to the last entry.
    kMaxValue = ZIP_OR_STATE_REQUIREMENT_VIOLATED,
  };

  // Represents the status of the field type requirements that are specific to
  // countries.
  enum class AddressProfileImportCountrySpecificFieldRequirementsMetric {
    ALL_GOOD = 0,
    ZIP_REQUIREMENT_VIOLATED = 1,
    STATE_REQUIREMENT_VIOLATED = 2,
    ZIP_STATE_REQUIREMENT_VIOLATED = 3,
    CITY_REQUIREMENT_VIOLATED = 4,
    ZIP_CITY_REQUIREMENT_VIOLATED = 5,
    STATE_CITY_REQUIREMENT_VIOLATED = 6,
    ZIP_STATE_CITY_REQUIREMENT_VIOLATED = 7,
    LINE1_REQUIREMENT_VIOLATED = 8,
    LINE1_ZIP_REQUIREMENT_VIOLATED = 9,
    LINE1_STATE_REQUIREMENT_VIOLATED = 10,
    LINE1_ZIP_STATE_REQUIREMENT_VIOLATED = 11,
    LINE1_CITY_REQUIREMENT_VIOLATED = 12,
    LINE1_ZIP_CITY_REQUIREMENT_VIOLATED = 13,
    LINE1_STATE_CITY_REQUIREMENT_VIOLATED = 14,
    LINE1_ZIP_STATE_CITY_REQUIREMENT_VIOLATED = 15,
    kMaxValue = LINE1_ZIP_STATE_CITY_REQUIREMENT_VIOLATED,
  };

  // To record if the value in an autofilled field was edited by the user.
  enum class AutofilledFieldUserEditingStatusMetric {
    AUTOFILLED_FIELD_WAS_EDITED = 0,
    AUTOFILLED_FIELD_WAS_NOT_EDITED = 1,
    kMaxValue = AUTOFILLED_FIELD_WAS_NOT_EDITED,
  };

  // Represent the overall status of a profile import.
  enum class AddressProfileImportStatusMetric {
    NO_IMPORT = 0,
    REGULAR_IMPORT = 1,
    SECTION_UNION_IMPORT = 2,
    kMaxValue = SECTION_UNION_IMPORT,
  };

  // Utility to log URL keyed form interaction events.
  class FormInteractionsUkmLogger {
   public:
    FormInteractionsUkmLogger(ukm::UkmRecorder* ukm_recorder,
                              const ukm::SourceId source_id);

    bool has_pinned_timestamp() const { return !pinned_timestamp_.is_null(); }
    void set_pinned_timestamp(base::TimeTicks t) { pinned_timestamp_ = t; }

    // Initializes this logger with a source_id. Unless forms is parsed no
    // autofill UKM is recorded. However due to autofill_manager resets,
    // it is possible to have the UKM being recorded after the forms were
    // parsed. So, rely on autofill_client to pass correct source_id
    // However during some cases there is a race for setting AutofillClient
    // and generation of new source_id (by UKM) as they are both observing tab
    // navigation. Ideally we need to refactor ownership of this logger
    // so as not to rely on OnFormsParsed to record the metrics correctly.
    // TODO(nikunjb): Refactor the logger to be owned by AutofillClient.
    void OnFormsParsed(const ukm::SourceId source_id);
    void LogInteractedWithForm(bool is_for_credit_card,
                               size_t local_record_type_count,
                               size_t server_record_type_count,
                               FormSignature form_signature);
    void LogSuggestionsShown(const FormStructure& form,
                             const AutofillField& field,
                             const base::TimeTicks& form_parsed_timestamp,
                             bool off_the_record);
    void LogDidFillSuggestion(int record_type,
                              bool is_for_credit_card,
                              const FormStructure& form,
                              const AutofillField& field);
    void LogTextFieldDidChange(const FormStructure& form,
                               const AutofillField& field);
    void LogEditedAutofilledFieldAtSubmission(const FormStructure& form,
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
                          bool has_upi_vpa_field,
                          const std::set<FormType>& form_types,
                          AutofillFormSubmittedState state,
                          const base::TimeTicks& form_parsed_timestamp,
                          FormSignature form_signature);
    void LogFormEvent(FormEvent form_event,
                      const std::set<FormType>& form_types,
                      const base::TimeTicks& form_parsed_timestamp);

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
    ukm::SourceId source_id_;
    base::TimeTicks pinned_timestamp_;
  };

  // Utility class to pin the timestamp used by the FormInteractionsUkmLogger
  // while an instance of this class is in scope. Pinned timestamps cannot be
  // nested.
  class UkmTimestampPin {
   public:
    explicit UkmTimestampPin(FormInteractionsUkmLogger* logger);
    ~UkmTimestampPin();

   private:
    FormInteractionsUkmLogger* const logger_;
    DISALLOW_IMPLICIT_CONSTRUCTORS(UkmTimestampPin);
  };

  // When the autofill-use-improved-label-disambiguation experiment is enabled
  // and suggestions are available, records if a LabelFormatter successfully
  // created the suggestions.
  static void LogProfileSuggestionsMadeWithFormatter(bool made_with_formatter);

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

  // When local card migration is not offered due to max strike limit reached,
  // logs the occurrence.
  static void LogLocalCardMigrationNotOfferedDueToMaxStrikesMetric(
      SaveTypeMetric metric);

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

  // |upload_decision_metrics| is a bitmask of |CardUploadDecisionMetric|.
  static void LogCardUploadDecisionMetrics(int upload_decision_metrics);
  static void LogCreditCardInfoBarMetric(
      InfoBarMetric metric,
      bool is_uploading,
      AutofillClient::SaveCreditCardOptions options,
      int previous_save_credit_card_prompt_user_decision);
  static void LogCreditCardFillingInfoBarMetric(InfoBarMetric metric);
  static void LogSaveCardRequestExpirationDateReasonMetric(
      SaveCardRequestExpirationDateReasonMetric metric);
  static void LogSaveCardPromptOfferMetric(
      SaveCardPromptOfferMetric metric,
      bool is_uploading,
      bool is_reshow,
      AutofillClient::SaveCreditCardOptions options,
      int previous_save_credit_card_prompt_user_decision,
      security_state::SecurityLevel security_level,
      AutofillSyncSigninState sync_state);
  static void LogSaveCardPromptResultMetric(
      SaveCardPromptResultMetric metric,
      bool is_uploading,
      bool is_reshow,
      AutofillClient::SaveCreditCardOptions options,
      int previous_save_credit_card_prompt_user_decision,
      security_state::SecurityLevel security_level,
      AutofillSyncSigninState sync_state);
  static void LogSaveCardPromptMetric(
      SaveCardPromptMetric metric,
      bool is_uploading,
      bool is_reshow,
      AutofillClient::SaveCreditCardOptions options,
      int previous_save_credit_card_prompt_user_decision,
      security_state::SecurityLevel security_level,
      AutofillSyncSigninState sync_state);
  static void LogSaveCardPromptMetricBySecurityLevel(
      SaveCardPromptMetric metric,
      bool is_uploading,
      security_state::SecurityLevel security_level);
  static void LogCreditCardUploadLegalMessageLinkClicked();
  static void LogCreditCardUploadFeedbackMetric(
      CreditCardUploadFeedbackMetric metric);
  static void LogManageCardsPromptMetric(ManageCardsPromptMetric metric,
                                         bool is_uploading);
  static void LogScanCreditCardPromptMetric(ScanCreditCardPromptMetric metric);
  static void LogLocalCardMigrationDecisionMetric(
      LocalCardMigrationDecisionMetric metric);
  static void LogLocalCardMigrationBubbleOfferMetric(
      LocalCardMigrationBubbleOfferMetric metric,
      bool is_reshow);
  // TODO(crbug.com/1070799): Delete the user interaction metrics when the
  // experiment is fully launched.
  static void LogLocalCardMigrationBubbleUserInteractionMetric(
      LocalCardMigrationBubbleUserInteractionMetric metric,
      bool is_reshow);
  static void LogLocalCardMigrationBubbleResultMetric(
      LocalCardMigrationBubbleResultMetric metric,
      bool is_reshow);
  static void LogLocalCardMigrationDialogOfferMetric(
      LocalCardMigrationDialogOfferMetric metric);
  static void LogLocalCardMigrationDialogUserInteractionMetric(
      const base::TimeDelta& duration,
      LocalCardMigrationDialogUserInteractionMetric metric);
  static void LogLocalCardMigrationDialogUserSelectionPercentageMetric(
      int selected,
      int total);
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
      security_state::SecurityLevel security_level,
      uint32_t profile_form_bitmask);

  static void LogUserHappinessMetric(
      UserHappinessMetric metric,
      const std::set<FormType>& form_types,
      security_state::SecurityLevel security_level,
      uint32_t profile_form_bitmask);

  static void LogUserHappinessBySecurityLevel(
      UserHappinessMetric metric,
      FormType form_type,
      security_state::SecurityLevel security_level);

  static void LogUserHappinessByProfileFormType(UserHappinessMetric metric,
                                                uint32_t profile_form_bitmask);

  // Logs the card fetch latency after a WebAuthn prompt.
  static void LogCardUnmaskDurationAfterWebauthn(
      const base::TimeDelta& duration,
      AutofillClient::PaymentsRpcResult result);

  // Logs the count of calls to PaymentsClient::GetUnmaskDetails() (aka
  // GetDetailsForGetRealPan).
  static void LogCardUnmaskPreflightCalled();

  // Logs the duration of the PaymentsClient::GetUnmaskDetails() call (aka
  // GetDetailsForGetRealPan).
  static void LogCardUnmaskPreflightDuration(const base::TimeDelta& duration);

  // Logs the count of calls to PaymentsClient::OptChange() (aka
  // UpdateAutofillUserPreference).
  static void LogWebauthnOptChangeCalled(bool request_to_opt_in,
                                         bool is_checkout_flow,
                                         WebauthnOptInParameters metric);

  // Logs the number of times the opt-in promo for enabling FIDO authentication
  // for card unmasking has been shown.
  static void LogWebauthnOptInPromoShown(bool is_checkout_flow);

  // Logs the user response to the opt-in promo for enabling FIDO authentication
  // for card unmasking.
  static void LogWebauthnOptInPromoUserDecision(
      bool is_checkout_flow,
      WebauthnOptInPromoUserDecisionMetric metric);

  // Logs which unmask type was used for a user with FIDO authentication
  // enabled.
  static void LogCardUnmaskTypeDecision(CardUnmaskTypeDecisionMetric metric);

  // Logs the existence of any user-perceived latency between selecting a Google
  // Payments server card and seeing a card unmask prompt.
  static void LogUserPerceivedLatencyOnCardSelection(PreflightCallEvent event,
                                                     bool fido_auth_enabled);

  // Logs the duration of any user-perceived latency between selecting a Google
  // Payments server card and seeing a card unmask prompt (CVC or FIDO).
  static void LogUserPerceivedLatencyOnCardSelectionDuration(
      const base::TimeDelta duration);

  // Logs whether or not the verifying pending dialog timed out between
  // selecting a Google Payments server card and seeing a card unmask prompt.
  static void LogUserPerceivedLatencyOnCardSelectionTimedOut(bool did_time_out);

  // Logs the duration of WebAuthn's
  // IsUserVerifiablePlatformAuthenticatorAvailable() call. It is supposedly an
  // extremely quick IPC.
  static void LogUserVerifiabilityCheckDuration(
      const base::TimeDelta& duration);

  // Logs the result of a WebAuthn prompt.
  static void LogWebauthnResult(WebauthnFlowEvent event,
                                WebauthnResultMetric metric);

  // Logs |event| to the unmask prompt events histogram.
  static void LogUnmaskPromptEvent(UnmaskPromptEvent event,
                                   bool has_valid_nickname);

  // Logs |event| to cardholder name fix flow prompt events histogram.
  static void LogCardholderNameFixFlowPromptEvent(
      CardholderNameFixFlowPromptEvent event);

  // Logs |event| to expiration date fix flow prompt events histogram.
  static void LogExpirationDateFixFlowPromptEvent(
      ExpirationDateFixFlowPromptEvent event);

  // Logs the count of expiration date fix flow prompts shown histogram.
  static void LogExpirationDateFixFlowPromptShown();

  // Logs the time elapsed between the unmask prompt being shown and it
  // being closed.
  static void LogUnmaskPromptEventDuration(const base::TimeDelta& duration,
                                           UnmaskPromptEvent close_event,
                                           bool has_valid_nickname);

  // Logs the time elapsed between the user clicking Verify and
  // hitting cancel when abandoning a pending unmasking operation
  // (aka GetRealPan).
  static void LogTimeBeforeAbandonUnmasking(const base::TimeDelta& duration,
                                            bool has_valid_nickname);

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
  static void LogIsAutofillEnabledAtPageLoad(
      bool enabled,
      AutofillSyncSigninState sync_state);

  // This should be called each time a page containing forms is loaded.
  static void LogIsAutofillProfileEnabledAtPageLoad(
      bool enabled,
      AutofillSyncSigninState sync_state);

  // This should be called each time a page containing forms is loaded.
  static void LogIsAutofillCreditCardEnabledAtPageLoad(
      bool enabled,
      AutofillSyncSigninState sync_state);

  // This should be called each time a new chrome profile is launched.
  static void LogIsAutofillEnabledAtStartup(bool enabled);

  // This should be called each time a new chrome profile is launched.
  static void LogIsAutofillProfileEnabledAtStartup(bool enabled);

  // This should be called each time a new chrome profile is launched.
  static void LogIsAutofillCreditCardEnabledAtStartup(bool enabled);

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

  // Logs metrics about the offer data associated with a profile. This should be
  // called each time a chrome profile is launched.
  static void LogStoredOfferMetrics(
      const std::vector<std::unique_ptr<AutofillOfferData>>& offers);

  // Logs whether the synced autofill offer data is valid.
  static void LogSyncedOfferDataBeingValid(bool invalid);

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
  static void LogAutofillSuggestionAcceptedIndex(int index,
                                                 PopupType popup_type,
                                                 bool off_the_record);

  // Logs that the user cleared the form.
  static void LogAutofillFormCleared();

  // Log the number of days since an Autocomplete suggestion was last used.
  static void LogAutocompleteDaysSinceLastUse(size_t days);

  // Log the index of the selected Autocomplete suggestion in the popup.
  static void LogAutocompleteSuggestionAcceptedIndex(int index);

  // Log the fact that an autocomplete popup was shown.
  static void OnAutocompleteSuggestionsShown();

  // Log the number of autocomplete entries that were cleaned-up as a result
  // of the Autocomplete Retention Policy.
  static void LogNumberOfAutocompleteEntriesCleanedUp(int nb_entries);

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
      bool has_upi_vpa_field,
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
      mojom::SubmissionSource submission_source);

  // Logs whether or not an upload |was_sent| after being triggered by a
  // |submission_source| event.
  static void LogUploadEvent(mojom::SubmissionSource submission_source,
                             bool was_sent);

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

  // Converts form type to bit vector to store in UKM.
  static int64_t FormTypesToBitVector(const std::set<FormType>& form_types);

  // Records the fact that the server card link was clicked with information
  // about the current sync state.
  static void LogServerCardLinkClicked(AutofillSyncSigninState sync_state);

  // Records whether the user had opted-in to seeing their server card when in
  // sync transport mode for Wallet. This should only be logged if the user had
  // server cards to be shown.
  static void LogWalletSyncTransportCardsOptIn(bool is_opted_in);

  // Records the reason for why (or why not) card upload was enabled for the
  // user.
  static void LogCardUploadEnabledMetric(CardUploadEnabledMetric metric,
                                         AutofillSyncSigninState sync_state);

  // Logs the status of an address import requirement defined by type.
  static void LogAddressFormImportRequirementMetric(
      AutofillMetrics::AddressProfileImportRequirementMetric metric);

  // Logs the overall status of the country specific field requirements for
  // importing an address profile from a submitted form.
  static void LogAddressFormImportCountrySpecificFieldRequirementsMetric(
      bool is_zip_missing,
      bool is_state_missing,
      bool is_city_missing,
      bool is_line1_missing);

  // Records if an autofilled field of a specific type was edited by the user.
  static void LogEditedAutofilledFieldAtSubmission(
      FormInteractionsUkmLogger* form_interactions_ukm_logger,
      const FormStructure& form,
      const AutofillField& field);

  static void LogAddressFormImportStatustMetric(
      AddressProfileImportStatusMetric metric);

  // Records if the page was translated upon form submission.
  static void LogFieldParsingPageTranslationStatusMetric(bool metric);

  // Records the visible page language upon form submission.
  static void LogFieldParsingTranslatedFormLanguageMetric(base::StringPiece);

  static const char* GetMetricsSyncStateSuffix(
      AutofillSyncSigninState sync_state);

  // Records whether a document collected phone number, and/or used WebOTP,
  // and/or used OneTimeCode (OTC) during its lifecycle.
  static void LogWebOTPPhoneCollectionMetricStateUkm(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      uint32_t phone_collection_metric_state);

 private:
  static void Log(AutocompleteEvent event);

  static const int kNumCardUploadDecisionMetrics = 19;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AutofillMetrics);
};

#if defined(UNIT_TEST)
int GetFieldTypeUserEditStatusMetric(
    ServerFieldType server_type,
    AutofillMetrics::AutofilledFieldUserEditingStatusMetric metric);
#endif

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_METRICS_H_
