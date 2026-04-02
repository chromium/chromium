// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"

namespace views {
class ImageView;
}  // namespace views

namespace gfx {
class ImageSkia;
}

namespace ui {
class ImageModel;
}

namespace payments {



/// Creates an image view for an icon in the SPC transactions details table.
std::unique_ptr<views::ImageView> CreateSecurePaymentConfirmationIconView(
    const gfx::ImageSkia& image);

/// Creates an image view for an icon in the SPC dialog using an ImageModel.
std::unique_ptr<views::ImageView> CreateSecurePaymentConfirmationIconView(
    const ui::ImageModel& image);

// Formats the merchant label by combining the name and origin for display.
std::u16string FormatMerchantLabel(
    const std::optional<std::u16string>& merchant_name,
    const std::optional<std::u16string>& merchant_origin);


}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_
