// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_H_

#include <stddef.h>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/security_state/core/security_state.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class GURL;

namespace ukm::builders {
class Autofill_CreditCardFill;
}

namespace autofill {

class AutofillField;
class CreditCard;

namespace autofill_metrics {
class FormEventLoggerBase;
}  // namespace autofill_metrics

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

    // An Autocomplete suggestion was deleted. Added in M113.
    AUTOCOMPLETE_SUGGESTION_DELETED,

    NUM_AUTOCOMPLETE_EVENTS
  };

  // The user action that triggered the deletion of an Autocomplete entry.
  // These values are used in enums.xml; do not reorder or renumber entries!
  enum class AutocompleteSingleEntryRemovalMethod {
    // The user pressed shift delete while an Autofill popup menu entry was
    // selected.
    kKeyboardShiftDeletePressed = 0,
    // The user clicked the delete button in the Autofill popup menu.
    kDeleteButtonClicked = 1,
    kMaxValue = kDeleteButtonClicked
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

  // Metric to distinguish between local and server saves for credit cards or
  // IBANs.
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

  // Metrics to track events in CardUnmaskAuthenticationSelectionDialog.
  enum class CardUnmaskAuthenticationSelectionDialogResultMetric {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.

    // Default value, should never be used.
    kUnknown = 0,
    // User canceled the dialog before selecting a challenge option.
    kCanceledByUserBeforeSelection = 1,
    // User canceled the dialog after selecting a challenge option, while the
    // dialog was in a pending state.
    kCanceledByUserAfterSelection = 2,
    // Server success after user chose a challenge option. For instance, in the
    // SMS OTP case, a server success indicates that the server successfully
    // requested the issuer to send an OTP, and we can move on to the next step
    // in this flow.
    kDismissedByServerRequestSuccess = 3,
    // Server failure after user chose a challenge option. For instance, in the
    // SMS OTP case, a server failure indicates that the server unsuccessfully
    // requested the issuer to send an OTP, and we can not move on to the next
    // step in this flow.
    kDismissedByServerRequestFailure = 4,
    // User accepted a challenge option in the
    // CardUnmaskAuthenticationSelectionDialog that does not require a server
    // call to get to the next step in this flow. For instance, in the CVC case,
    // we can go directly to the CVC input dialog after the user selects the
    // challenge option.
    kDismissedByUserAcceptanceNoServerRequestNeeded = 5,
    kMaxValue = kDismissedByUserAcceptanceNoServerRequestNeeded,
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
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum UnmaskPromptEvent {
    // The prompt was shown.
    UNMASK_PROMPT_SHOWN = 0,
    // The prompt was closed without attempting to unmask the card.
    UNMASK_PROMPT_CLOSED_NO_ATTEMPTS = 1,
    // The prompt was closed without unmasking the card, but with at least
    // one attempt. The last failure was retriable.
    UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE = 2,
    // The prompt was closed without unmasking the card, but with at least
    // one attempt. The last failure was non retriable.
    UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE = 3,
    // Successfully unmasked the card in the first attempt.
    UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT = 4,
    // Successfully unmasked the card after retriable failures.
    UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS = 5,
    // Saved the card locally (masked card was upgraded to a full card).
    // Deprecated.
    // UNMASK_PROMPT_SAVED_CARD_LOCALLY = 6,
    // User chose to opt in (checked the checkbox when it was empty).
    // Only logged if there was an attempt to unmask.
    // Deprecated.
    // UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_IN = 7,
    // User did not opt in when they had the chance (left the checkbox
    // unchecked).  Only logged if there was an attempt to unmask.
    // Deprecated.
    // UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_IN = 8,
    // User chose to opt out (unchecked the checkbox when it was check).
    // Only logged if there was an attempt to unmask.
    // Deprecated.
    // UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_OUT = 9,
    // User did not opt out when they had a chance (left the checkbox checked).
    // Only logged if there was an attempt to unmask.
    // Deprecated.
    // UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_OUT = 10,
    // The prompt was closed while chrome was unmasking the card (user pressed
    // verify and we were waiting for the server response).
    UNMASK_PROMPT_CLOSED_ABANDON_UNMASKING = 11,
    NUM_UNMASK_PROMPT_EVENTS,
  };

  // Possible results of Payments RPCs.
  enum PaymentsRpcResult {
    // Request succeeded.
    PAYMENTS_RESULT_SUCCESS = 0,
    // Request failed; try again.
    PAYMENTS_RESULT_TRY_AGAIN_FAILURE = 1,
    // Request failed; don't try again.
    PAYMENTS_RESULT_PERMANENT_FAILURE = 2,
    // Unable to connect to Payments servers.
    PAYMENTS_RESULT_NETWORK_ERROR = 3,
    // Request failed in virtual card information retrieval; try again.
    PAYMENTS_RESULT_VCN_RETRIEVAL_TRY_AGAIN_FAILURE = 4,
    // Request failed in virtual card information retrieval; don't try again.
    PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE = 5,
    kMaxValue = PAYMENTS_RESULT_VCN_RETRIEVAL_PERMANENT_FAILURE,
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
    // A fatal error occurred while communicating with the Wallet server. This
    // value has been deprecated.
    WALLET_FATAL_ERROR_DEPRECATED,
    // Received a malformed response from the Wallet server.
    WALLET_MALFORMED_RESPONSE,
    // A network error occurred while communicating with the Wallet server.
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
  // server. This is similar to the wallet::RequiredAction enum; but unlike
  // that enum, the values in this one must remain constant over time, so that
  // the metrics can be consistently interpreted on the server-side.
  enum WalletRequiredActionMetric {
    // Baseline metric: Issued a request to the Wallet server.
    WALLET_REQUIRED_ACTION_BASELINE_ISSUED_REQUEST = 0,
    // Values from the wallet::RequiredAction enum:
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

  // To record if the value in an autofilled field was edited by the user.
  enum class AutofilledFieldUserEditingStatusMetric {
    AUTOFILLED_FIELD_WAS_EDITED = 0,
    AUTOFILLED_FIELD_WAS_NOT_EDITED = 1,
    kMaxValue = AUTOFILLED_FIELD_WAS_NOT_EDITED,
  };

  // The filling status of an autofilled field.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FieldFillingStatus {
    // The field was filled and accepted.
    kAccepted = 0,
    // The field was filled and corrected to a value of the same type.
    kCorrectedToSameType = 1,
    // The field was filled and corrected to a value of a different type.
    kCorrectedToDifferentType = 2,
    // The field was filled and corrected to a value of an unknown type.
    kCorrectedToUnknownType = 3,
    // The field was filled and the value was cleared afterwards.
    kCorrectedToEmpty = 4,
    // The field was manually filled to a value of the same type as the
    // field was predicted to.
    kManuallyFilledToSameType = 5,
    // The field was manually filled to a value of a different type as the field
    // was predicted to.
    kManuallyFilledToDifferentType = 6,
    // The field was manually filled to a value of an unknown type.
    kManuallyFilledToUnknownType = 7,
    // The field was left empty.
    kLeftEmpty = 8,
    kMaxValue = kLeftEmpty
  };

  enum class AutocompleteState {
    kNone = 0,
    kValid = 1,
    kGarbage = 2,
    kOff = 3,
    kMaxValue = kOff
  };

  // The autofill statuses of a field that are recorded into UKM to help us
  // understand the autofill performance and user behaviors.
  enum class AutofillStatus {
    kIsFocusable = 0,
    kWasFocused = 1,
    kWasAutofillTriggered = 2,
    // kWasAutofilled is only set when kWasAutofillTriggered is set.
    kWasAutofilled = 3,
    kWasRefill = 4,
    // The below suggestion statuses are set only when kWasFocused is set.
    kSuggestionWasAvailable = 5,
    kSuggestionWasShown = 6,
    kSuggestionWasAccepted = 7,
    kUserTypedIntoField = 8,
    kFilledValueWasModified = 9,
    kHadValueBeforeFilling = 10,
    kHadTypedOrFilledValueAtSubmission = 11,
    kIsInSubFrame = 12,
    kMaxValue = kIsInSubFrame
  };

  struct FormEventSetTraits {
    static constexpr autofill_metrics::FormEvent kMinValue =
        autofill_metrics::FormEvent(0);
    static constexpr autofill_metrics::FormEvent kMaxValue =
        autofill_metrics::NUM_FORM_EVENTS;
    static constexpr bool kPacked = false;
  };

  using FormEventSet =
      DenseSet<autofill_metrics::FormEvent, FormEventSetTraits>;

  // Utility class for determining the seamlessness of a credit card fill.
  class CreditCardSeamlessness {
   public:
    // A qualitative representation of a fill seamlessness.
    //
    // Keep consistent with FORM_EVENT_CREDIT_CARD_SEAMLESSNESS_*.
    //
    // The different meaning of the categories is as follows:
    enum class Metric {                // | Name | Number | Exp Date | CVC |
      kFullFill = 0,                   // |  X   |   X    |    X     |  X  |
      kOptionalNameMissing = 1,        // |      |   X    |    X     |  X  |
      kOptionalCvcMissing = 2,         // |  X   |   X    |    X     |     |
      kOptionalNameAndCvcMissing = 3,  // |      |   X    |    X     |     |
      kFullFillButExpDateMissing = 4,  // |  X   |   X    |          |  X  |
      kPartialFill = 5,                // |           otherwise            |
      kMaxValue = kPartialFill,
    };

    explicit CreditCardSeamlessness(const ServerFieldTypeSet& filled_types);

    explicit operator bool() const { return is_valid(); }
    bool is_valid() const { return name_ || number_ || exp_ || cvc_; }

    Metric QualitativeMetric() const;

    uint64_t QualitativeMetricAsInt() const {
      return static_cast<uint64_t>(QualitativeMetric());
    }

    // TODO(crbug.com/1275953): Remove once the new UKM metric has gained
    // traction.
    autofill_metrics::FormEvent QualitativeFillableFormEvent() const;
    autofill_metrics::FormEvent QualitativeFillFormEvent() const;

    // Returns a four-bit bitmask.
    uint8_t BitmaskMetric() const;

    static uint8_t BitmaskExclusiveMax() { return true << 4; }

   private:
    bool name_ = false;
    bool number_ = false;
    bool exp_ = false;
    bool cvc_ = false;
  };

  // Utility to log URL keyed form interaction events.
  class FormInteractionsUkmLogger {
   public:
    FormInteractionsUkmLogger(AutofillClient* autofill_client,
                              ukm::UkmRecorder* ukm_recorder);

    bool has_pinned_timestamp() const { return !pinned_timestamp_.is_null(); }
    void set_pinned_timestamp(base::TimeTicks t) { pinned_timestamp_ = t; }

    ukm::builders::Autofill_CreditCardFill CreateCreditCardFillBuilder();
    void Record(ukm::builders::Autofill_CreditCardFill&& builder);

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
    void LogDidFillSuggestion(absl::variant<AutofillProfile::RecordType,
                                            CreditCard::RecordType> record_type,
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
    void LogAutofillFieldInfoAtFormRemove(
        const FormStructure& form,
        const AutofillField& field,
        AutofillMetrics::AutocompleteState autocomplete_state);
    void LogAutofillFormSummaryAtFormRemove(
        const FormStructure& form_structure,
        FormEventSet form_events,
        const base::TimeTicks& initial_interaction_timestamp,
        const base::TimeTicks& form_submitted_timestamp);
    void LogFormSubmitted(bool is_for_credit_card,
                          bool has_upi_vpa_field,
                          const DenseSet<FormType>& form_types,
                          AutofillFormSubmittedState state,
                          const base::TimeTicks& form_parsed_timestamp,
                          FormSignature form_signature,
                          const FormInteractionCounts& form_interaction_counts);
    void LogKeyMetrics(const DenseSet<FormType>& form_types,
                       bool data_to_fill_available,
                       bool suggestions_shown,
                       bool edited_autofilled_field,
                       bool suggestion_filled,
                       const FormInteractionCounts& form_interaction_counts,
                       const FormInteractionsFlowId& flow_id,
                       absl::optional<int64_t> fast_checkout_run_id);
    void LogFormEvent(autofill_metrics::FormEvent form_event,
                      const DenseSet<FormType>& form_types,
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

    // Logs a hash of the `sectioning_signature` for a specific
    // `form_signature`. This is useful for detecting sites where different
    // sectioning algorithms yield different results. Emitted every time
    // sectioning is performed and only when
    // `AutofillUseParameterizedSectioning` is enabled.
    void LogSectioningHash(FormSignature form_signature,
                           uint32_t sectioning_signature);

   private:
    bool CanLog() const;
    int64_t MillisecondsSinceFormParsed(
        const base::TimeTicks& form_parsed_timestamp) const;

    ukm::SourceId GetSourceId();

    // These objects outlive.
    raw_ptr<AutofillClient> autofill_client_;
    raw_ptr<ukm::UkmRecorder> ukm_recorder_;

    absl::optional<ukm::SourceId> source_id_;
    base::TimeTicks pinned_timestamp_;
  };

  // Utility class to pin the timestamp used by the FormInteractionsUkmLogger
  // while an instance of this class is in scope. Pinned timestamps cannot be
  // nested.
  class UkmTimestampPin {
   public:
    UkmTimestampPin() = delete;

    explicit UkmTimestampPin(FormInteractionsUkmLogger* logger);

    UkmTimestampPin(const UkmTimestampPin&) = delete;
    UkmTimestampPin& operator=(const UkmTimestampPin&) = delete;

    ~UkmTimestampPin();

   private:
    const raw_ptr<FormInteractionsUkmLogger> logger_;
  };

  enum class PredictionState {
    kNone = 0,
    kServer = 1,
    kHeuristics = 2,
    kBoth = 3,
    kMaxValue = kBoth
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PaymentsSigninState {
    // The user is not signed in to Chromium.
    kSignedOut = 0,
    // The user is signed in to Chromium.
    kSignedIn = 1,
    // The user is signed in to Chromium and sync transport is active for Wallet
    // data.
    kSignedInAndWalletSyncTransportEnabled = 2,
    // The user is signed in, has enabled the sync feature and has not disabled
    // Wallet sync.
    kSignedInAndSyncFeatureEnabled = 3,
    // The user has enabled the sync feature, but has then signed out, so sync
    // is paused.
    kSyncPaused = 4,
    kUnknown = 5,
    kMaxValue = kUnknown
  };

  AutofillMetrics() = delete;
  AutofillMetrics(const AutofillMetrics&) = delete;
  AutofillMetrics& operator=(const AutofillMetrics&) = delete;

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

  // When credit card upload is offered, logs whether the card being offered is
  // already a local card on the device or not.
  static void LogUploadOfferedCardOriginMetric(
      UploadOfferedCardOriginMetric metric);

  // When credit card upload is accepted, logs whether the card being accepted
  // is already a local card on the device or not.
  static void LogUploadAcceptedCardOriginMetric(
      UploadAcceptedCardOriginMetric metric);

  // When the Card Unmask Authentication Selection Dialog is shown, logs the
  // result of what the user did with the dialog.
  static void LogCardUnmaskAuthenticationSelectionDialogResultMetric(
      CardUnmaskAuthenticationSelectionDialogResultMetric metric);

  // Logs the number of challenge options shown every time the Card Unmask
  // Authentication Selection Dialog is shown.
  static void LogCardUnmaskAuthenticationSelectionDialogShown(
      size_t number_of_challenge_options);

  static void LogCreditCardInfoBarMetric(
      InfoBarMetric metric,
      bool is_uploading,
      AutofillClient::SaveCreditCardOptions options);
  static void LogCreditCardFillingInfoBarMetric(InfoBarMetric metric);
  static void LogScanCreditCardPromptMetric(ScanCreditCardPromptMetric metric);
  static void LogProgressDialogResultMetric(
      bool is_canceled_by_user,
      AutofillProgressDialogType autofill_progress_dialog_type);
  static void LogProgressDialogShown(
      AutofillProgressDialogType autofill_progress_dialog_type);

  // Returns a string representation of the given AutofillProgressDialogType for
  // constructing subhistogram paths.
  static std::string_view GetDialogTypeStringForLogging(
      AutofillProgressDialogType autofill_progress_dialog_type);

  // Should be called when credit card scan is finished. |duration| should be
  // the time elapsed between launching the credit card scanner and getting back
  // the result. |completed| should be true if a credit card was scanned, false
  // if the scan was cancelled.
  static void LogScanCreditCardCompleted(const base::TimeDelta& duration,
                                         bool completed);

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
      const DenseSet<FormType>& form_types,
      security_state::SecurityLevel security_level,
      uint32_t profile_form_bitmask);

  static void LogUserHappinessBySecurityLevel(
      UserHappinessMetric metric,
      FormType form_type,
      security_state::SecurityLevel security_level);

  static void LogUserHappinessByProfileFormType(UserHappinessMetric metric,
                                                uint32_t profile_form_bitmask);

  // Logs |event| to the unmask prompt events histogram.
  static void LogUnmaskPromptEvent(UnmaskPromptEvent event,
                                   bool has_valid_nickname,
                                   CreditCard::RecordType card_type);

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

  // Logs |result| to the get real pan result histogram. |card_type| indicates
  // the type of the credit card that the request fetched.
  static void LogRealPanResult(AutofillClient::PaymentsRpcResult result,
                               AutofillClient::PaymentsRpcCardType card_type);

  // Logs |result| to duration of the GetRealPan RPC. |card_type| indicates the
  // type of the credit card that the request fetched.
  static void LogRealPanDuration(const base::TimeDelta& duration,
                                 AutofillClient::PaymentsRpcResult result,
                                 AutofillClient::PaymentsRpcCardType card_type);

  // Logs |result| to the get real pan result histogram. |card_type| indicates
  // the type of the credit card that the request fetched.
  static void LogUnmaskingDuration(
      const base::TimeDelta& duration,
      AutofillClient::PaymentsRpcResult result,
      AutofillClient::PaymentsRpcCardType card_type);

  // This should be called when a form that has been Autofilled is submitted.
  // |duration| should be the time elapsed between form load and submission.
  static void LogFormFillDurationFromLoadWithAutofill(
      const base::TimeDelta& duration);

  // This should be called when a fillable form that has not been Autofilled is
  // submitted.  |duration| should be the time elapsed between form load and
  // submission.
  static void LogFormFillDurationFromLoadWithoutAutofill(
      const base::TimeDelta& duration);

  // This should be called when a form with |autocomplete="one-time-code"| is
  // submitted. |duration| should be the time elapsed between form load and
  // submission.
  static void LogFormFillDurationFromLoadForOneTimeCode(
      const base::TimeDelta& duration);

  // This should be called when a form is submitted. |duration| should be the
  // time elapsed between the initial form interaction and submission. This
  // metric is sliced by |form_type| and |used_autofill|.
  static void LogFormFillDurationFromInteraction(
      const DenseSet<FormType>& form_types,
      bool used_autofill,
      const base::TimeDelta& duration);

  // This should be called when a form with |autocomplete="one-time-code"| is
  // submitted. |duration| should be the time elapsed between the initial form
  // interaction and submission.
  static void LogFormFillDurationFromInteractionForOneTimeCode(
      const base::TimeDelta& duration);

  static void LogFormFillDuration(const std::string& metric,
                                  const base::TimeDelta& duration);

  // This should be called each time a page containing forms is loaded.
  static void LogIsAutofillEnabledAtPageLoad(bool enabled,
                                             PaymentsSigninState sync_state);

  // This should be called each time a page containing forms is loaded.
  static void LogIsAutofillProfileEnabledAtPageLoad(
      bool enabled,
      PaymentsSigninState sync_state);

  // This should be called each time a page containing forms is loaded.
  static void LogIsAutofillCreditCardEnabledAtPageLoad(
      bool enabled,
      PaymentsSigninState sync_state);

  // This should be called each time a new chrome profile is launched.
  static void LogIsAutofillEnabledAtStartup(bool enabled);

  // This should be called each time a new chrome profile is launched.
  static void LogIsAutofillProfileEnabledAtStartup(bool enabled);

  // This should be called each time a new chrome profile is launched.
  static void LogIsAutofillCreditCardEnabledAtStartup(bool enabled);

  // Logs various metrics about the local and server cards associated with a
  // profile. This should be called each time a new chrome profile is launched.
  static void LogStoredCreditCardMetrics(
      const std::vector<std::unique_ptr<CreditCard>>& local_cards,
      const std::vector<std::unique_ptr<CreditCard>>& server_cards,
      size_t server_card_count_with_card_art_image,
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

  // Log the reason for which the Autofill popup disappeared.
  static void LogAutofillPopupHidingReason(PopupHidingReason reason);

  // Logs that the user cleared the form.
  static void LogAutofillFormCleared();

  // Logs that the user used Undo to revert some autofill operation.
  static void LogAutofillUndo();

  // Log the number of days since an Autocomplete suggestion was last used.
  static void LogAutocompleteDaysSinceLastUse(size_t days);

  // Log the index of the selected Autocomplete suggestion in the popup.
  static void LogAutocompleteSuggestionAcceptedIndex(int index);

  // Log the fact that an autocomplete popup was shown.
  static void OnAutocompleteSuggestionsShown();

  // Log that an autocomplete suggestion was deleted directly from the popup
  // menu.
  static void OnAutocompleteSuggestionDeleted(
      AutocompleteSingleEntryRemovalMethod removal_method);

  // Log how many autofilled fields in a given form were edited before the
  // submission or when the user unfocused the form (depending on
  // |observed_submission|).
  static void LogNumberOfEditedAutofilledFields(
      size_t num_edited_autofilled_fields,
      bool observed_submission);

  // Logs the number of sections and the number of fields/section.
  static void LogSectioningMetrics(
      const base::flat_map<Section, size_t>& fields_per_section);

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
      const DenseSet<FormType>& form_types,
      const base::TimeTicks& form_parsed_timestamp,
      FormSignature form_signature,
      FormInteractionsUkmLogger* form_interactions_ukm_logger,
      const FormInteractionCounts& form_interaction_counts);

  // Logs if every non-empty field in a submitted form was filled by Autofill.
  // If |is_address| an address was filled, otherwise it was a credit card.
  static void LogAutofillPerfectFilling(bool is_address, bool perfect_filling);

  struct LogCreditCardSeamlessnessParam {
    const raw_ref<autofill_metrics::FormEventLoggerBase> event_logger;
    const raw_ref<const FormStructure> form;
    const raw_ref<const AutofillField> field;
    const raw_ref<const base::flat_set<FieldGlobalId>> newly_filled_fields;
    const raw_ref<const base::flat_set<FieldGlobalId>> safe_fields;
    const raw_ref<ukm::builders::Autofill_CreditCardFill> builder;
  };

  // Logs several metrics about seamlessness. These are qualitative and bitmask
  // UMA and UKM metrics as well as a UKM metric indicating whether
  // "shared-autofill" did or would make a difference.
  //
  // The metrics are:
  // - UMA metrics "Autofill.CreditCard.Seamless{Fillable,Fills}.AtFillTime
  //   {Before,After}SecurityPolicy[.Bitmask]".
  // - UKM event "Autofill.CreditCardSeamlessness".
  // - UKM event "Autofill.FormEvent" for FORM_EVENT_CREDIT_CARD_*.
  static void LogCreditCardSeamlessnessAtFillTime(
      const LogCreditCardSeamlessnessParam& p);

  // Logs Autofill.CreditCard.SeamlessFills.AtSubmissionTime.
  static void LogCreditCardSeamlessnessAtSubmissionTime(
      const ServerFieldTypeSet& autofilled_types);

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
                                        DenseSet<FormType> form_types,
                                        int developer_engagement_metrics,
                                        FormSignature form_signature);

  // Log the number of hidden or presentational 'select' fields that were
  // autofilled to support synthetic fields.
  static void LogHiddenOrPresentationalSelectFieldsFilled();

  // Converts form type to bit vector to store in UKM.
  static int64_t FormTypesToBitVector(const DenseSet<FormType>& form_types);

  // Records the fact that the server card link was clicked with information
  // about the current sync state.
  static void LogServerCardLinkClicked(PaymentsSigninState sync_state);

  // Records if an autofilled field of a specific type was edited by the user.
  // TODO(crbug.com/1368096): This metric is the successor of
  // LogEditedAutofilledFieldAtSubmissionDeprecated which is defective. Remove
  // comment once the old metric was removed.
  static void LogEditedAutofilledFieldAtSubmission(
      FormInteractionsUkmLogger* form_interactions_ukm_logger,
      const FormStructure& form,
      const AutofillField& field);

  // Records if an autofilled field of a specific type was edited by the user.
  // TODO(crbug.com/1368096): This metric is defective because it is falsely
  // conditioned on having a detected field type. Remove after M112.
  static void LogEditedAutofilledFieldAtSubmissionDeprecated(
      FormInteractionsUkmLogger* form_interactions_ukm_logger,
      const FormStructure& form,
      const AutofillField& field);

  // Records if the page was translated upon form submission.
  static void LogFieldParsingPageTranslationStatusMetric(bool metric);

  // Records the visible page language upon form submission.
  static void LogFieldParsingTranslatedFormLanguageMetric(base::StringPiece);

  static const char* GetMetricsSyncStateSuffix(PaymentsSigninState sync_state);

  // Records whether a document collected phone number, and/or used WebOTP,
  // and/or used OneTimeCode (OTC) during its lifecycle.
  static void LogWebOTPPhoneCollectionMetricStateUkm(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      uint32_t phone_collection_metric_state);

  // Logs the number of autofilled fields at submission time.
  static void LogNumberOfAutofilledFieldsAtSubmission(
      size_t number_of_accepted_fields,
      size_t number_of_corrected_fields);

  // Logs the number of autofilled fields with unrecognized autocomplete
  // attribute at submission time.
  static void
  LogNumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission(
      size_t number_of_accepted_fields,
      size_t number_of_corrected_fields);

  // Logs that local heuristics matched phone number fields using `grammar_id`.
  // `suffix_matched` indicates if the special case handling for phone number
  // suffixes was triggered.
  // `num_grammars` indicates the total number of phone number grammars. It is
  // not logged and used for validation.
  static void LogPhoneNumberGrammarMatched(int grammar_id,
                                           bool suffix_matched,
                                           int num_grammars);

  // Logs when the virtual card metadata for one card have been updated.
  static void LogVirtualCardMetadataSynced(bool existing_card);

  // Logs the verification status of non-empty name-related profile tokens when
  // a profile is used to fill a form.
  static void LogVerificationStatusOfNameTokensOnProfileUsage(
      const AutofillProfile& profile);

  // Logs the verification status of non-empty address-related profile tokens
  // when a profile is used to fill a form.
  static void LogVerificationStatusOfAddressTokensOnProfileUsage(
      const AutofillProfile& profile);

  // Logs the image fetching result for one image in AutofillImageFetcher.
  static void LogImageFetchResult(bool succeeded);
  // Logs the roundtrip latency for fetching an image in AutofillImageFetcher.
  static void LogImageFetcherRequestLatency(const base::TimeDelta& latency);

  // Logs whether the submitted field value is same as the non-empty value
  // to be autofilled in the field, when the field had a different prefilled
  // value.
  static void LogIsValueNotAutofilledOverExistingValueSameAsSubmittedValue(
      bool is_same);

  // Logs a field's (PredictionState, AutocompleteState) pair on form submit.
  static void LogAutocompletePredictionCollisionState(
      PredictionState prediction_state,
      AutocompleteState autocomplete_state);

  // Logs a field's server and heuristic type on form submit into a histogram
  // corresponding to the field's `autocomplete_state`.
  static void LogAutocompletePredictionCollisionTypes(
      AutocompleteState autocomplete_state,
      ServerFieldType server_type,
      ServerFieldType heuristic_types);

  // Logs whether a heuristic detection for an NUMERIC_QUANTITY collides with a
  // server prediction.
  static void LogNumericQuantityCollidesWithServerPrediction(bool collision);

  // Logs if the filling of a field was accepted even though it had a
  // NUMERIC_QUANTITY. This metric is only emitted if the feature to grant the
  // heuristic precedence is disabled.
  static void LogAcceptedFilledFieldWithNumericQuantityHeuristicPrediction(
      bool accepted);

  // Returns the histogram string for the passed in
  // `AutofillClient::PaymentsRpcCardType` or `CreditCard::RecordType`, starting
  // with a period.
  static std::string GetHistogramStringForCardType(
      absl::variant<AutofillClient::PaymentsRpcCardType, CreditCard::RecordType>
          card_type);

  // Logs the context menu impressions based on the autofill type as well as
  // based on the autocomplete type.
  static void LogContextMenuImpressionsForField(
      ServerFieldType field_type,
      AutocompleteState autocomplete_state);

  // Logs the context menu impressions for a submitted form. Mainly logs the
  // number of fields in the form where the context menu was shown.
  static void LogContextMenuImpressionsForForm(
      int num_of_fields_with_context_menu_shown);

  // Returns 64-bit hash of the string of form global id, which consists of
  // |frame_token| and |renderer_id|.
  static uint64_t FormGlobalIdToHash64Bit(const FormGlobalId& form_global_id);
  // Returns 64-bit hash of the string of field global id, which consists of
  // |frame_token| and |renderer_id|.
  static uint64_t FieldGlobalIdToHash64Bit(
      const FieldGlobalId& field_global_id);

  // Log the Autofill2_FieldInfoAfterSubmission UKM event after the form is
  // submitted and uploaded for votes to the crowdsourcing server.
  static void LogAutofillFieldInfoAfterSubmission(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id,
      const FormStructure& form,
      const base::TimeTicks& form_submitted_timestamp);

 private:
  static void Log(AutocompleteEvent event);
};

#if defined(UNIT_TEST)
int GetFieldTypeUserEditStatusMetric(
    ServerFieldType server_type,
    AutofillMetrics::AutofilledFieldUserEditingStatusMetric metric);
#endif

std::string GetCreditCardTypeSuffix(
    AutofillClient::PaymentsRpcCardType card_type);

const std::string PaymentsRpcResultToMetricsSuffix(
    AutofillClient::PaymentsRpcResult result);

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_H_
