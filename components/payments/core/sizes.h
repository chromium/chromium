// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_SIZES_H_
#define COMPONENTS_PAYMENTS_CORE_SIZES_H_

namespace payments {

// The default width of icons for the SPC Transaction UX. Used for any fallback
// icons that may be shown if the caller-supplied icon isn't available.
inline constexpr int kSecurePaymentConfirmationIconDefaultWidthPx = 20;

// The maximum width of icons in the SPC Transaction UX. Used as the preferred
// size of downloaded icons.
inline constexpr int kSecurePaymentConfirmationIconMaximumWidthPx = 32;

// The default height of icons for the SPC Transaction UX.
inline constexpr int kSecurePaymentConfirmationIconHeightPx = 20;

// The height of the default header logo in the SPC Transaction UX.
inline constexpr int kSecurePaymentConfirmationDefaultHeaderLogoHeight = 80;

// The width of the header logo in the SPC Transaction UX.
inline constexpr int kSecurePaymentConfirmationHeaderLogoWidth = 130;

// The height of the header logo in the SPC Transaction UX.
inline constexpr int kSecurePaymentConfirmationHeaderLogoHeight = 30;

// The size of the error icon in the Payment Handler error message view.
inline constexpr int kPaymentHandlerErrorIconSize = 24;

// The top inset of the error message row in the Payment Handler error message
// view.
inline constexpr int kPaymentHandlerErrorRowTopInset = 24;

// The top inset of the loading message in the payment app loading view.
inline constexpr int kPaymentAppLoadingViewMessageTopInset = 120;

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_SIZES_H_
