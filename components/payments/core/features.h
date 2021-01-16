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

// Used to control whether the invoking TWA can handle payments for app store
// payment method identifiers.
extern const base::Feature kAppStoreBilling;

// Used to control whether to remove the restriction that TWA has to be
// installed from specific app stores.
extern const base::Feature kAppStoreBillingDebug;

// Used to control whether allow crawling just-in-time installable payment app.
extern const base::Feature kWebPaymentsJustInTimePaymentApp;

// Used to enable crawling just-in-time installable payment apps even if
// basic-card is also requested.
extern const base::Feature kAlwaysAllowJustInTimePaymentApp;

// Used to control whether the shipping address returned for the
// ShippingAddressChangeEvent is redacted of fine-grained details.
extern const base::Feature kWebPaymentsRedactShippingAddress;

// Used to make autofill instrument more restrictive when responding to
// hasEnrolledInstrument() queries.
extern const base::Feature kStrictHasEnrolledAutofillInstrument;

// Enables skip-to-GPay experimental flow.
extern const base::Feature kPaymentRequestSkipToGPay;

// Enables skip-to-GPay experimental flow, but only if user doesn't have an
// eligible credit card.
extern const base::Feature kPaymentRequestSkipToGPayIfNoCard;

// If enabled, just-in-time installable payment handlers are ranked lower than
// complete autofill instruments in payment sheet's method selection section.
extern const base::Feature kDownRankJustInTimePaymentApp;

// Desktop only, if enabled payment handler window size matches the pop up
// window size.
extern const base::Feature kPaymentHandlerPopUpSizeWindow;

// Used to test icon refetch for JIT installed apps with missing icons.
extern const base::Feature kAllowJITInstallationWhenAppIconIsMissing;

// Desktop only, if enabled the security icon would be showed next to the
// payment handler's URL bar. It indicate that only secure content is
// allowed inside the payment handler.
extern const base::Feature kPaymentHandlerSecurityIcon;

// Used to reject the apps with partial delegation.
extern const base::Feature kEnforceFullDelegation;

// Browser-side feature flag for SecurePaymentConfirmation, which can be used to
// disable the feature. The feature is also controlled by the Blink runtime
// feature "SecurePaymentConfirmation". Both have to be enabled for
// SecurePaymentConfirmation to be available.
extern const base::Feature kSecurePaymentConfirmation;

}  // namespace features
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_FEATURES_H_
