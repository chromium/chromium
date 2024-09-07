// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CREDIT_CARD_SAVE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CREDIT_CARD_SAVE_METRICS_H_

#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::autofill_metrics {

// The total number of values in the |CardUploadDecisionMetric| enum. Must be
// updated each time a new value is added.
const int kNumCardUploadDecisionMetrics = 19;

enum CardUploadDecision {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

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
  // The card does not satisfy any of the ranges of supported BIN ranges.
  UPLOAD_NOT_OFFERED_UNSUPPORTED_BIN_RANGE = 1 << 16,
  // The legal message was invalid.
  UPLOAD_NOT_OFFERED_INVALID_LEGAL_MESSAGE = 1 << 18,
  // Update |kNumCardUploadDecisionMetrics| when adding new enum here.
};

// Log all the scenarios that contribute to the decision of whether card
// upload is enabled or not.
enum class CardUploadEnabled {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  kSyncServiceNull = 0,
  kSyncServicePaused = 1,
  kSyncServiceMissingAutofillWalletDataActiveType = 2,
  kSyncServiceMissingAutofillSelectedType = 3,
  // Deprecated: kAccountWalletStorageUploadDisabled = 4,
  kUsingExplicitSyncPassphrase = 5,
  kLocalSyncEnabled = 6,
  // Deprecated: kPaymentsIntegrationDisabled = 7,
  // Deprecated: kEmailEmpty = 8,
  // Deprecated: kEmailDomainNotSupported = 9,
  // Deprecated: kAutofillUpstreamDisabled = 10,
  // Deprecated: kCardUploadEnabled = 11,
  kUnsupportedCountry = 12,
  kEnabledForCountry = 13,
  kEnabledByFlag = 14,
  kMaxValue = kEnabledByFlag,
};

// Metrics to track event when the save card prompt is offered.
enum class SaveCardPromptOffer {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The prompt is actually shown.
  kShown = 0,
  // The prompt is not shown because the prompt has been declined by the user
  // too many times.
  kNotShownMaxStrikesReached = 1,
  // The prompt is not shown because the required delay since last strike has
  // not passed.
  kNotShownRequiredDelay = 2,
  kMaxValue = kNotShownRequiredDelay,
};

enum class SaveCardPromptResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The user explicitly accepted the prompt by clicking the ok button.
  kAccepted = 0,
  // The user explicitly cancelled the prompt by clicking the cancel button.
  kCancelled = 1,
  // The user explicitly closed the prompt with the close button or ESC.
  kClosed = 2,
  // The user did not interact with the prompt.
  kNotInteracted = 3,
  // The prompt lost focus and was deactivated.
  kLostFocus = 4,
  // The reason why the prompt is closed is not clear. Possible reason is the
  // logging function is invoked before the closed reason is correctly set.
  kUnknown = 5,
  kMaxValue = kUnknown,
};

// Represents requesting expiration date reason.
enum class SaveCardRequestExpirationDateReason {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // Submitted card has empty month.
  kMonthMissingOnly = 0,
  // Submitted card has empty year.
  kYearMissingOnly = 1,
  // Submitted card has both empty month and year.
  kMonthAndYearMissing = 2,
  // Submitted card has expired expiration date.
  kExpirationDatePresentButExpired = 3,
  kMaxValue = kExpirationDatePresentButExpired,
};

// Clank-specific metrics.
enum class SaveCreditCardPromptResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // User accepted save.
  kAccepted = 0,
  // User declined to save card.
  kDenied = 1,
  // User did not interact with the flow.
  kIgnored = 2,
  // User interacted but then ignored, without explicitly accepting or
  // cancelling.
  kInteractedAndIgnored = 3,
  kMaxValue = kInteractedAndIgnored,
};

// |upload_decision_metrics| is a bitmask of |CardUploadDecisionMetric|.
void LogCardUploadDecisionMetrics(int upload_decision_metrics);

// Logs the card upload decisions ukm for the specified |url|.
// |upload_decision_metrics| is a bitmask of |CardUploadDecisionMetric|.
void LogCardUploadDecisionsUkm(ukm::UkmRecorder* ukm_recorder,
                               ukm::SourceId source_id,
                               const GURL& url,
                               int upload_decision_metrics);

// Records the reason for why (or why not) card upload was enabled for the
// user.
void LogCardUploadEnabledMetric(
    CardUploadEnabled metric,
    AutofillMetrics::PaymentsSigninState sync_state);

// When credit card save is not offered (either at all on mobile or by simply
// not showing the bubble on desktop), logs the occurrence.
void LogCreditCardSaveNotOfferedDueToMaxStrikesMetric(
    AutofillMetrics::SaveTypeMetric metric);

void LogCreditCardUploadLegalMessageLinkClicked();

// When a cardholder name fix flow is shown during credit card upload, logs
// whether the cardholder name was prefilled or not.
void LogSaveCardCardholderNamePrefilled(bool prefilled);

// When a cardholder name fix flow is shown during credit card upload and the
// user accepts upload, logs whether the final cardholder name was changed
// from its prefilled value or not.
void LogSaveCardCardholderNameWasEdited(bool edited);

void LogSaveCardPromptOfferMetric(
    SaveCardPromptOffer metric,
    bool is_uploading,
    bool is_reshow,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options,
    AutofillMetrics::PaymentsSigninState sync_state);

// `has_saved_cards` indicates that local or server cards existed before the
// save prompt was accepted/denied.
void LogSaveCardPromptResultMetric(
    SaveCardPromptResult metric,
    bool is_uploading,
    bool is_reshow,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options,
    AutofillMetrics::PaymentsSigninState sync_state,
    bool has_saved_cards);

void LogSaveCvcPromptOfferMetric(SaveCardPromptOffer metric,
                                 bool is_uploading,
                                 bool is_reshow);

void LogSaveCvcPromptResultMetric(SaveCardPromptResult metric,
                                  bool is_uploading,
                                  bool is_reshow);

void LogCvcInfoBarMetric(AutofillMetrics::InfoBarMetric metric,
                         bool is_uploading);

void LogSaveCardRequestExpirationDateReasonMetric(
    SaveCardRequestExpirationDateReason metric);

void LogCreditCardUploadRanLocalSaveFallbackMetric(bool new_local_card_added);

void LogCreditCardUploadLoadingViewShownMetric(bool is_shown);

void LogCreditCardUploadConfirmationViewShownMetric(bool is_shown,
                                                    bool is_card_uploaded);

void LogCreditCardUploadLoadingViewResultMetric(SaveCardPromptResult metric);

void LogCreditCardUploadConfirmationViewResultMetric(
    SaveCardPromptResult metric,
    bool is_card_uploaded);

void LogGetCardUploadDetailsRequestLatencyMetric(base::TimeDelta duration,
                                                 bool is_successful);

// Clank-specific metrics.
void LogSaveCreditCardPromptResult(
    SaveCreditCardPromptResult event,
    bool is_upload,
    payments::PaymentsAutofillClient::SaveCreditCardOptions options);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_CREDIT_CARD_SAVE_METRICS_H_
