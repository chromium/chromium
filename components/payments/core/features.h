// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_FEATURES_H_
#define COMPONENTS_PAYMENTS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"

namespace payments {
namespace features {

// Master toggle for all experimental features that will ship in the next
// release.
BASE_DECLARE_FEATURE(kWebPaymentsExperimentalFeatures);

// Used to control whether the Payment Sheet can be skipped for Payment Requests
// with a single URL based payment app and no other info requested.
BASE_DECLARE_FEATURE(kWebPaymentsSingleAppUiSkip);

// Used to control whether the invoking TWA can handle payments for app store
// payment method identifiers.
BASE_DECLARE_FEATURE(kAppStoreBilling);

// Used to control whether to remove the restriction that TWA has to be
// installed from specific app stores.
BASE_DECLARE_FEATURE(kAppStoreBillingDebug);

// Used to control whether allow crawling just-in-time installable payment app.
BASE_DECLARE_FEATURE(kWebPaymentsJustInTimePaymentApp);

// Used to test icon refetch for JIT installed apps with missing icons.
BASE_DECLARE_FEATURE(kAllowJITInstallationWhenAppIconIsMissing);

// Used to reject the apps with partial delegation.
BASE_DECLARE_FEATURE(kEnforceFullDelegation);

// If enabled, the GooglePayPaymentApp handles communications between the native
// GPay app and the browser for dynamic updates on shipping and payment data.
BASE_DECLARE_FEATURE(kGPayAppDynamicUpdate);

// Used to control whether SecurePaymentConfirmation is able to rely on OS-level
// credential store APIs, or if it can only rely on the user-profile database.
BASE_DECLARE_FEATURE(kSecurePaymentConfirmationUseCredentialStoreAPIs);

#if !BUILDFLAG(IS_ANDROID)
// Desktop only, if enabled the Task Manager will show the PaymentHandler
// window.
BASE_DECLARE_FEATURE(kPaymentHandlerWindowInTaskManager);
#endif

// If enabled, the web-app manifest for already-installed service-worker apps
// will always be refetched for every Payment Request, in order to potentially
// refresh the icon for the app.
BASE_DECLARE_FEATURE(kPaymentHandlerAlwaysRefreshIcon);

// If enabled, the payment method manifest fetch for Payment Handler must go via
// a Link header with rel="payment-method-manifest".
BASE_DECLARE_FEATURE(kPaymentHandlerRequireLinkHeader);

#if BUILDFLAG(USE_BLINK)
// Controls how network and issuer icons (when enabled) are presented in SPC UX.
extern const base::FeatureParam<std::string>
    kSecurePaymentConfirmationNetworkAndIssuerIconsOptions;

// Defines the supported UX treatments for displaying the network and issuer
// icons in SPC UX.
enum class SecurePaymentConfirmationNetworkAndIssuerIconsTreatment {
  // Issuer and network icons should not be shown.
  kNone,
  // Issuer and network icons should be shown inline with the dialog title text.
  kInline,
  // Issuer and network icons should be shown as rows in the SPC transaction
  // data 'table'.
  kRows
};

// Retrieve the current UX treatment for network and issuer icons for SPC, based
// on the feature flags set.
SecurePaymentConfirmationNetworkAndIssuerIconsTreatment
GetNetworkAndIssuerIconsTreatment();
#endif

}  // namespace features
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_FEATURES_H_
