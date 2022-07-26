// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace payments {
namespace features {

const base::Feature kWebPaymentsExperimentalFeatures{
    "WebPaymentsExperimentalFeatures", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_IOS)
const base::Feature kWebPaymentsNativeApps{"WebPaymentsNativeApps",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// TODO(rouslan): Remove this.
const base::Feature kWebPaymentsMethodSectionOrderV2{
    "WebPaymentsMethodSectionOrderV2", base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(rouslan): Remove this.
const base::Feature kWebPaymentsModifiers{"WebPaymentsModifiers",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(rouslan): Remove this.
const base::Feature kWebPaymentsSingleAppUiSkip{
    "WebPaymentsSingleAppUiSkip", base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(rouslan): Remove this.
const base::Feature kWebPaymentsJustInTimePaymentApp{
    "WebPaymentsJustInTimePaymentApp", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWebPaymentsRedactShippingAddress{
    "WebPaymentsRedactShippingAddress", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAppStoreBilling {
  "AppStoreBilling",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kAppStoreBillingDebug{"AppStoreBillingDebug",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPaymentHandlerPopUpSizeWindow{
    "PaymentHandlerPopUpSizeWindow", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAllowJITInstallationWhenAppIconIsMissing{
    "AllowJITInstallationWhenAppIconIsMissing",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnforceFullDelegation{"EnforceFullDelegation",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kGPayAppDynamicUpdate{"GPayAppDynamicUpdate",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace payments
