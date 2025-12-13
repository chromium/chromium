// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/features.h"

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

BASE_FEATURE(kCanMakePaymentTrueWhenPrivate, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowJITInstallationWhenAppIconIsMissing,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnforceFullDelegation, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGPayAppDynamicUpdate, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRestrictIsReadyToPayQuery, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSecurePaymentConfirmationUseCredentialStoreAPIs,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSecurePaymentConfirmationFallback,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace payments
