// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_MANDATORY_REAUTH_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_MANDATORY_REAUTH_METRICS_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill {

namespace payments {
enum class MandatoryReauthAuthenticationMethod;
}

enum class NonInteractivePaymentMethodType;

namespace autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MandatoryReauthOfferOptInDecision {
  // Opt-in is offered.
  kOffered = 0,
  // Opt-in is not offered in incognito mode.
  kIncognitoMode = 1,
  // The user does not have a valid auth method on the device (e.g. screenlock),
  // so opt-in will not be offered.
  kNoSupportedReauthMethod = 2,
  // Deprecated: kNoCardExtractedFromForm = 3,
  // We only offer opt-in whenever the user experiences a non-interactive
  // authentication.
  kWentThroughInteractiveAuthentication = 4,
  // For corner cases when a user goes through a non-interactive authentication
  // flow with a card that is not a local/server/virtual card, then types in a
  // local/server/virtual card manually into the form.
  // Deprecated: kManuallyFilledLocalCard = 5,
  // Deprecated: kManuallyFilledServerCard = 6,
  // Deprecated: kManuallyFilledVirtualCard = 7,
  // For corner cases when there is no stored card for the extracted card.
  // Deprecated: kNoStoredCardForExtractedCard = 8,
  // Currently reauth opt-in is only supported for local and virtual cards.
  // Deprecated: kUnsupportedCardType = 9,
  // Opt-in is never re-offered once the user has opted in or out.
  kAlreadyOptedIn = 10,
  kAlreadyOptedOut = 11,
  // Opt-in is not offered if it is blocked by the strike database.
  kBlockedByStrikeDatabase = 12,
  kMaxValue = kBlockedByStrikeDatabase,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MandatoryReauthOptInBubbleOffer {
  // The user is shown the opt-in bubble.
  kShown = 0,
  kMaxValue = kShown,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MandatoryReauthOptInBubbleResult {
  // The reason why the bubble is closed is not clear. Possible reason is the
  // logging function is invoked before the closed reason is correctly set.
  kUnknown = 0,
  // The user explicitly accepted the bubble by clicking the ok button.
  kAccepted = 1,
  // The user explicitly cancelled the bubble by clicking the cancel button.
  kCancelled = 2,
  // The user explicitly closed the bubble with the close button or ESC.
  kClosed = 3,
  // The user did not interact with the bubble.
  kNotInteracted = 4,
  // The bubble lost focus and was deactivated.
  kLostFocus = 5,
  kMaxValue = kLostFocus,
};

enum class MandatoryReauthOptInConfirmationBubbleMetric {
  // The user is shown the opt-in confirmation bubble.
  kShown = 0,
  // The user clicks the settings link of the opt-in confirmation bubble.
  kSettingsLinkClicked = 1,
  kMaxValue = kSettingsLinkClicked,
};

// Enum class to include all the possible auth flows that can occur for
// mandatory reauth. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill
enum class MandatoryReauthAuthenticationFlowEvent {
  kUnknown = 0,
  // User authentication flow started.
  kFlowStarted = 1,
  // User authentication flow succeeded.
  kFlowSucceeded = 2,
  // User authentication flow failed.
  kFlowFailed = 3,
  // User authentication flow was skipped because of previous auth success.
  kFlowSkipped = 4,
  kMaxValue = kFlowSkipped,
};

// All the sources that can trigger the OptIn or OptOut flow for mandatory
// reauth.
enum class MandatoryReauthOptInOrOutSource {
  kUnknown = 0,
  // The OptIn or OptOut process is triggered from the settings page.
  kSettingsPage = 1,
  // The OptIn is triggered after using a local card during checkout.
  kCheckoutLocalCard = 2,
  // The OptIn is triggered after using a green-pathed virtual card during
  // checkout.
  kCheckoutVirtualCard = 3,
  // The OptIn is triggered after filling a full server card.
  kCheckoutFullServerCard = 4,
  // The OptIn is triggered after using a green-pathed masked server card during
  // checkout.
  kCheckoutMaskedServerCard = 5,
  // The OptIn is triggered after using a local IBAN during checkout.
  kCheckoutLocalIban = 6,
  // The OptIn is triggered after using a server IBAN during checkout.
  kCheckoutServerIban = 7,
  kMaxValue = kCheckoutServerIban,
};

void LogMandatoryReauthOfferOptInDecision(
    MandatoryReauthOfferOptInDecision opt_in_decision);

// Logs when the user is offered mandatory reauth.
void LogMandatoryReauthOptInBubbleOffer(MandatoryReauthOptInBubbleOffer metric,
                                        bool is_reshow);

// Logs when the user interacts with the opt-in bubble.
void LogMandatoryReauthOptInBubbleResult(
    MandatoryReauthOptInBubbleResult metric,
    bool is_reshow);

// Logs events related to the opt-in confirmation bubble.
void LogMandatoryReauthOptInConfirmationBubbleMetric(
    MandatoryReauthOptInConfirmationBubbleMetric metric);

// Logs all the possible flows for mandatory reauth during OptIn or OptOut
// process.
// We check the status of the mandatory reauth feature to determine if the
// user is trying to opt in or out.
// If mandatory reauth is currently on, and the user is trying to turn it off
// then the bool `opt_in` will be false.
// If mandatory reauth is currently off, and the user is trying to turn it on
// then the bool `opt_in` will be true.
void LogMandatoryReauthOptInOrOutUpdateEvent(
    MandatoryReauthOptInOrOutSource source,
    bool opt_in,
    MandatoryReauthAuthenticationFlowEvent event);

// Logs the status of a mandatory reauth occurrence, such as flow
// started/succeeded/failed, when the user tries to edit a local card on the
// Settings page.
void LogMandatoryReauthSettingsPageEditCardEvent(
    MandatoryReauthAuthenticationFlowEvent event);

// Logs the status of a mandatory reauth occurrence, such as flow
// started/succeeded/failed, when the user tries to delete a local card on the
// Settings page.
void LogMandatoryReauthSettingsPageDeleteCardEvent(
    MandatoryReauthAuthenticationFlowEvent event);

// Logs the status of a mandatory reauth occurrence during checkout flow, such
// as flow started/succeeded/failed, broken down by
// `non_interactive_payment_method_type` and `authentication_method`.
void LogMandatoryReauthCheckoutFlowUsageEvent(
    NonInteractivePaymentMethodType non_interactive_payment_method_type,
    payments::MandatoryReauthAuthenticationMethod authentication_method,
    MandatoryReauthAuthenticationFlowEvent event);

}  // namespace autofill_metrics

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_MANDATORY_REAUTH_METRICS_H_
