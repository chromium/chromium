// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_FEATURES_H_
#define COMPONENTS_PAYMENTS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace payments {
namespace features {

// Used to control whether Google Pay cards are returned for basic-card.
extern const base::Feature kReturnGooglePayInBasicCard;

#if defined(OS_IOS)
// Used to control the support for iOS third party apps as payment methods.
extern const base::Feature kWebPaymentsNativeApps;
#endif

// Used to control payment method section order on payment request UI. Payment
// method section should be put on top of the address section when this feature
// is enabled instead of under it.
extern const base::Feature kWebPaymentsMethodSectionOrderV2;

// Used to control the support for Payment Details modifiers.
extern const base::Feature kWebPaymentsModifiers;

// Used to control whether the Payment Sheet can be skipped for Payment Requests
// with a single URL based payment app and no other info requested.
extern const base::Feature kWebPaymentsSingleAppUiSkip;

// Used to control whether allow crawling just-in-time installable payment app.
extern const base::Feature kWebPaymentsJustInTimePaymentApp;

}  // namespace features
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_FEATURES_H_
