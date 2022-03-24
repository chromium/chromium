// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_SIZES_H_
#define COMPONENTS_PAYMENTS_CORE_SIZES_H_

namespace payments {

// The default width of the instrument icon for SPC. Based on the aspect ratio
// of the default instrument icon.
constexpr int kSecurePaymentConfirmationInstrumentIconDefaultWidthPx = 26;

// The maximum width of the instrument icon for SPC. Used as the preferred size
// of downloaded instrument icons.
constexpr int kSecurePaymentConfirmationInstrumentIconMaximumWidthPx = 32;

// The default height of the instrument icon for SPC.
constexpr int kSecurePaymentConfirmationInstrumentIconHeightPx = 20;

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_SIZES_H_
