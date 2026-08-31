// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_FEATURES_H_
#define COMPONENTS_PAYMENTS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

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

// Delays the dimming dialog background when the UI is skipped.
BASE_DECLARE_FEATURE(kDelayNativePaymentAppScrimShow);

// Used to reject the apps with partial delegation.
BASE_DECLARE_FEATURE(kEnforceFullDelegation);

// If enabled, the GooglePayPaymentApp handles communications between the native
// GPay app and the browser for dynamic updates on shipping and payment data.
BASE_DECLARE_FEATURE(kGPayAppDynamicUpdate);

// Approach for discovering available Secure Payment Confirmation credentials.
// Different platforms use different approaches, due to differing capabilities
// of the authenticators in use by SPC.
enum class CredentialDiscoveryMode {
  // Query only the local profile database.
  kUserDatabaseOnly = 0,
  // Query both the OS credential store and the local profile database in
  // parallel. The OS credential store results take precedence if available.
  kHybrid = 1,
  // Query only the OS credential store.
  kOsOnly = 2,
};

// Returns the string representation for a CredentialDiscoveryMode.
constexpr const char* CredentialDiscoveryModeToString(
    CredentialDiscoveryMode mode) {
  switch (mode) {
    case CredentialDiscoveryMode::kUserDatabaseOnly:
      return "database-only";
    case CredentialDiscoveryMode::kHybrid:
      return "hybrid";
    case CredentialDiscoveryMode::kOsOnly:
      return "os-only";
  }
}

// Controls the approach for discovering available Secure Payment Confirmation
// credentials.
BASE_DECLARE_FEATURE(kSecurePaymentConfirmationCredentialDiscoveryMode);

extern const base::FeatureParam<CredentialDiscoveryMode>
    kCredentialDiscoveryModeParam;

// Used to control whether SecurePaymentConfirmation stores newly created
// credentials in the OS-level credential store (skipping saving to the
// user-profile database).
BASE_DECLARE_FEATURE(kSecurePaymentConfirmationStoreCredentialsInOS);

// Used to control the usage of the renderer URL loader in the payment request.
BASE_DECLARE_FEATURE(kPaymentRequestUseRendererUrlLoader);

// Used to control whether Payment Request/Handler dialogs are rejected if the
// browser window is too small to contain them.
BASE_DECLARE_FEATURE(kPaymentRequestRejectTooSmallWindows);


// Used to control whether Payment Handler dialog includes an initiator during
// the URL load.
BASE_DECLARE_FEATURE(kPaymentHandlerDialogUseInitiatorInUrlLoad);

// Used to control whether to support HTML head <meta name="theme-color"> in
// Payment Handler dialog headers.
BASE_DECLARE_FEATURE(kPaymentHandlerHtmlHeadThemeColor);

// Used to control whether Payment Handler dialog requires user interaction
// before resolving a success payment response.
BASE_DECLARE_FEATURE(kPaymentRequestMandatoryPaymentAppUi);

// Used to control whether camera access is allowed in Payment Handler windows.
BASE_DECLARE_FEATURE(kPaymentHandlerCameraAccess);

// Used to control whether camera access with interactive permission prompt
// and indicator is allowed in Payment Handler windows.
BASE_DECLARE_FEATURE(kPaymentHandlerCameraAccessUx);

// Used to control whether SPC supports validating locale.
BASE_DECLARE_FEATURE(kSPCLocaleValidation);

// Used to control whether 3D-Secure telemetry is collected.
BASE_DECLARE_FEATURE(kThreeDSecureTelemetry);

}  // namespace features
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_FEATURES_H_
