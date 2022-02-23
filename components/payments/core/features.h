// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_FEATURES_H_
#define COMPONENTS_PAYMENTS_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace payments {
namespace features {

// Master toggle for all experimental features that will ship in the next
// release.
extern const base::Feature kWebPaymentsExperimentalFeatures;

#if BUILDFLAG(IS_IOS)
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

// Used to control whether the invoking TWA can handle payments for app store
// payment method identifiers.
extern const base::Feature kAppStoreBilling;

// Used to control whether to remove the restriction that TWA has to be
// installed from specific app stores.
extern const base::Feature kAppStoreBillingDebug;

// Used to control whether allow crawling just-in-time installable payment app.
extern const base::Feature kWebPaymentsJustInTimePaymentApp;

// Used to control whether the shipping address returned for the
// ShippingAddressChangeEvent is redacted of fine-grained details.
extern const base::Feature kWebPaymentsRedactShippingAddress;

// If enabled, just-in-time installable payment handlers are ranked lower than
// complete autofill instruments in payment sheet's method selection section.
extern const base::Feature kDownRankJustInTimePaymentApp;

// Desktop only, if enabled payment handler window size matches the pop up
// window size.
extern const base::Feature kPaymentHandlerPopUpSizeWindow;

// Used to test icon refetch for JIT installed apps with missing icons.
extern const base::Feature kAllowJITInstallationWhenAppIconIsMissing;

// Used to reject the apps with partial delegation.
extern const base::Feature kEnforceFullDelegation;

// If enabled, the GooglePayPaymentApp handles communications between the native
// GPay app and the browser for dynamic updates on shipping and payment data.
extern const base::Feature kGPayAppDynamicUpdate;

}  // namespace features
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_FEATURES_H_
