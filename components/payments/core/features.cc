// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace payments {
namespace features {

BASE_FEATURE(kWebPaymentsExperimentalFeatures,
             "WebPaymentsExperimentalFeatures",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kWebPaymentsNativeApps,
             "WebPaymentsNativeApps",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// TODO(rouslan): Remove this.
BASE_FEATURE(kWebPaymentsMethodSectionOrderV2,
             "WebPaymentsMethodSectionOrderV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(rouslan): Remove this.
BASE_FEATURE(kWebPaymentsModifiers,
             "WebPaymentsModifiers",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(rouslan): Remove this.
BASE_FEATURE(kWebPaymentsSingleAppUiSkip,
             "WebPaymentsSingleAppUiSkip",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(rouslan): Remove this.
BASE_FEATURE(kWebPaymentsJustInTimePaymentApp,
             "WebPaymentsJustInTimePaymentApp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebPaymentsRedactShippingAddress,
             "WebPaymentsRedactShippingAddress",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAppStoreBilling,
             "AppStoreBilling",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kAppStoreBillingDebug,
             "AppStoreBillingDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPaymentHandlerPopUpSizeWindow,
             "PaymentHandlerPopUpSizeWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kPaymentHandlerMinimalHeaderUX,
             "PaymentHandlerMinimalHeaderUX",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace payments
