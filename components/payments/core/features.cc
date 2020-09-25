// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/features.h"

#include "build/build_config.h"

namespace payments {
namespace features {

const base::Feature kWebPaymentsExperimentalFeatures{
    "WebPaymentsExperimentalFeatures", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReturnGooglePayInBasicCard{
    "ReturnGooglePayInBasicCard", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_IOS)
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

const base::Feature kAlwaysAllowJustInTimePaymentApp{
    "AlwaysAllowJustInTimePaymentApp", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebPaymentsRedactShippingAddress{
    "WebPaymentsRedactShippingAddress", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAppStoreBilling {
  "AppStoreBilling",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // OS_ANDROID
};

const base::Feature kAppStoreBillingDebug{"AppStoreBillingDebug",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kStrictHasEnrolledAutofillInstrument{
    "StrictHasEnrolledAutofillInstrument", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPaymentRequestSkipToGPay{
    "PaymentRequestSkipToGPay", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPaymentRequestSkipToGPayIfNoCard{
    "PaymentRequestSkipToGPayIfNoCard", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDownRankJustInTimePaymentApp{
    "DownRankJustInTimePaymentApp", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPaymentHandlerPopUpSizeWindow{
    "PaymentHandlerPopUpSizeWindow", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAllowJITInstallationWhenAppIconIsMissing{
    "AllowJITInstallationWhenAppIconIsMissing",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPaymentHandlerSecurityIcon{
    "PaymentHandlerSecurityIcon", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnforceFullDelegation{"EnforceFullDelegation",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSecurePaymentConfirmation {
  "SecurePaymentConfirmation",
#if defined(OS_MAC)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // OS_MAC
};

}  // namespace features
}  // namespace payments
