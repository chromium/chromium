// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/features.h"

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

const base::Feature kWebPaymentsPerMethodCanMakePaymentQuota{
    "WebPaymentsPerMethodCanMakePaymentQuota",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebPaymentsRedactShippingAddress{
    "WebPaymentsRedactShippingAddress", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kStrictHasEnrolledAutofillInstrument{
    "StrictHasEnrolledAutofillInstrument", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPaymentRequestSkipToGPay{
    "PaymentRequestSkipToGPay", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebPaymentMicrotransaction{
    "WebPaymentMicrotransaction", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace payments
