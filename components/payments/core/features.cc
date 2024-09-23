// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(USE_BLINK)
#include "third_party/blink/public/common/features_generated.h"
#endif

namespace payments {
namespace features {

BASE_FEATURE(kWebPaymentsExperimentalFeatures,
             "WebPaymentsExperimentalFeatures",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(rouslan): Remove this.
BASE_FEATURE(kWebPaymentsSingleAppUiSkip,
             "WebPaymentsSingleAppUiSkip",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(rouslan): Remove this.
BASE_FEATURE(kWebPaymentsJustInTimePaymentApp,
             "WebPaymentsJustInTimePaymentApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppStoreBilling,
             "AppStoreBilling",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kAppStoreBillingDebug,
             "AppStoreBillingDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowJITInstallationWhenAppIconIsMissing,
             "AllowJITInstallationWhenAppIconIsMissing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnforceFullDelegation,
             "EnforceFullDelegation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGPayAppDynamicUpdate,
             "GPayAppDynamicUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSecurePaymentConfirmationUseCredentialStoreAPIs,
             "SecurePaymentConfirmationUseCredentialStoreAPIs",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPaymentHandlerWindowInTaskManager,
             "PaymentHandlerWindowInTaskManager",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPaymentHandlerAlwaysRefreshIcon,
             "PaymentHandlerAlwaysRefreshIcon",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentHandlerRequireLinkHeader,
             "PaymentHandlerRequireLinkHeader",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(USE_BLINK)
const base::FeatureParam<std::string>
    kSecurePaymentConfirmationNetworkAndIssuerIconsOptions(
        &blink::features::kSecurePaymentConfirmationNetworkAndIssuerIcons,
        /*name=*/"spc_network_and_issuer_icons_option",
        /*default_value=*/"rows");

SecurePaymentConfirmationNetworkAndIssuerIconsTreatment
GetNetworkAndIssuerIconsTreatment() {
  if (!base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationNetworkAndIssuerIcons)) {
    return SecurePaymentConfirmationNetworkAndIssuerIconsTreatment::kNone;
  }

  std::string option =
      kSecurePaymentConfirmationNetworkAndIssuerIconsOptions.Get();
  if (option == "inline") {
    return SecurePaymentConfirmationNetworkAndIssuerIconsTreatment::kInline;
  } else if (option == "rows") {
    return SecurePaymentConfirmationNetworkAndIssuerIconsTreatment::kRows;
  }

  NOTREACHED();
}
#endif

}  // namespace features
}  // namespace payments
