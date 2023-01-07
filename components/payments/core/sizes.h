// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_SIZES_H_
#define COMPONENTS_PAYMENTS_CORE_SIZES_H_

namespace payments {

// The default width of the instrument icon for SPC. Based on the size of the
// default instrument icon.
constexpr int kSecurePaymentConfirmationInstrumentIconDefaultWidthPx = 20;

// The maximum width of the instrument icon for SPC. Used as the preferred size
// of downloaded instrument icons.
constexpr int kSecurePaymentConfirmationInstrumentIconMaximumWidthPx = 32;

// The default height of the instrument icon for SPC.
constexpr int kSecurePaymentConfirmationInstrumentIconHeightPx = 20;

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_SIZES_H_
