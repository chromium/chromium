// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/features.h"

namespace payments {
namespace features {

const base::Feature kReturnGooglePayInBasicCard{
    "ReturnGooglePayInBasicCard", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_IOS)
const base::Feature kWebPaymentsNativeApps{"WebPaymentsNativeApps",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kWebPaymentsMethodSectionOrderV2{
    "WebPaymentsMethodSectionOrderV2", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebPaymentsModifiers{"WebPaymentsModifiers",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWebPaymentsSingleAppUiSkip{
    "WebPaymentsSingleAppUiSkip", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWebPaymentsJustInTimePaymentApp{
    "WebPaymentsJustInTimePaymentApp", base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace features
}  // namespace payments
