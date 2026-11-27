// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/features.h"

#include <string>

#include "base/feature.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace payments {
namespace features {

BASE_FEATURE(kWebPaymentsExperimentalFeatures,
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(rouslan): Remove this.
BASE_FEATURE(kWebPaymentsSingleAppUiSkip, base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(rouslan): Remove this.
BASE_FEATURE(kWebPaymentsJustInTimePaymentApp,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppStoreBilling,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kAppStoreBillingDebug, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowJITInstallationWhenAppIconIsMissing,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDelayNativePaymentAppScrimShow, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnforceFullDelegation, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSecurePaymentConfirmationCredentialDiscoveryMode,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<CredentialDiscoveryMode>::Option
    kCredentialDiscoveryModeOptions[] = {
        {CredentialDiscoveryMode::kUserDatabaseOnly,
         CredentialDiscoveryModeToString(
             CredentialDiscoveryMode::kUserDatabaseOnly)},
        {CredentialDiscoveryMode::kHybrid,
         CredentialDiscoveryModeToString(CredentialDiscoveryMode::kHybrid)},
        {CredentialDiscoveryMode::kOsOnly,
         CredentialDiscoveryModeToString(CredentialDiscoveryMode::kOsOnly)},
};

const base::FeatureParam<CredentialDiscoveryMode> kCredentialDiscoveryModeParam{
    &kSecurePaymentConfirmationCredentialDiscoveryMode, "mode",
#if BUILDFLAG(IS_ANDROID)
    CredentialDiscoveryMode::kOsOnly,
#elif BUILDFLAG(IS_WIN)
    CredentialDiscoveryMode::kHybrid,
#else
    CredentialDiscoveryMode::kUserDatabaseOnly,
#endif
    &kCredentialDiscoveryModeOptions};

BASE_FEATURE(kSecurePaymentConfirmationStoreCredentialsInOS,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kPaymentRequestRejectTooSmallWindows,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentHandlerDialogUseInitiatorInUrlLoad,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentHandlerDialogUseFrameOriginAsInitiator,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentHandlerHtmlHeadThemeColor,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentRequestMandatoryPaymentAppUi,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentHandlerCameraAccessUx, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentHandlerDynamicCustomSize,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSPCLocaleValidation, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreeDSecureTelemetry, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreeDSecureTelemetryForIos, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentRequestEnforceLNAWithClientSecurityState,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to control whether user-initiated browser reloads on non-error
// documents reset the once-per-navigation activationless PaymentRequest.show()
// allowance.
BASE_FEATURE(kPaymentRequestResetActivationlessShowOnReload,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentHandlerPageInfoThemeColor,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCheckoutTelemetry, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kCheckoutTelemetryCheckoutUrlRegex{
    &kCheckoutTelemetry, "checkout_url_regex",
    "/checkout(?:[/?#]|$)"
    "|/checkouts(?:[/?#]|$)"
    "|/payment(?:[/?#]|$)"
    "|/shipping(?:[/?#]|$)"
    "|/place-order(?:[/?#]|$)"};

const base::FeatureParam<std::string>
    kCheckoutTelemetryOrderConfirmationUrlRegex{&kCheckoutTelemetry,
                                                "order_confirmation_url_regex",
                                                "/order-confirmation(?:[/?#]|$)"
                                                "|/order/success(?:[/?#]|$)"
                                                "|/thank-you(?:[/?#]|$)"
                                                "|/thankyou(?:[/?#]|$)"
                                                "|/order-placed(?:[/?#]|$)"
                                                "|/receipt(?:[/?#]|$)"};

const base::FeatureParam<std::string> kCheckoutTelemetryCheckoutTitleRegex{
    &kCheckoutTelemetry, "checkout_title_regex",
    R"(\bcheckout\b)"
    R"(|\bcheckouts\b)"
    R"(|\bpayment\b)"
    R"(|\bshipping\b)"
    R"(|\bplace[- ]order\b)"};

const base::FeatureParam<std::string>
    kCheckoutTelemetryOrderConfirmationTitleRegex{
        &kCheckoutTelemetry, "order_confirmation_title_regex",
        R"(\border[- ]confirmation\b)"
        R"(|\border[/ ]success\b)"
        R"(|\bthank[- ]you\b)"
        R"(|\bthankyou\b)"
        R"(|\border[- ]placed\b)"
        R"(|\breceipt\b)"};

}  // namespace features
}  // namespace payments
