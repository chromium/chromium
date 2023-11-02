// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace views {
class Label;
class ProgressBar;
class View;
class ImageView;
class StyledLabel;
}  // namespace views

namespace gfx {
class ImageSkia;
}

namespace payments {

// Height of the header icons.
constexpr int kHeaderIconHeight = 148;
constexpr int kShoppingCartHeaderIconHeight = 114;

// Padding above the header icon.
constexpr int kHeaderIconTopPadding = 12;

// Height of the progress bar at the top of the dialog.
constexpr int kProgressBarHeight = 4;

// Line height of the title text.
constexpr int kTitleLineHeight = 24;

// Line height of the description text.
constexpr int kDescriptionLineHeight = 20;

// Insets of the body content.
constexpr int kBodyInsets = 8;

// Insets of the secondary small text, e.g., the opt-out footer.
constexpr int kSecondarySmallTextInsets = 16;

// Extra inset between the body content and the dialog buttons.
constexpr int kBodyExtraInset = 16;

// Height of each payment information row.
constexpr int kPaymentInfoRowHeight = 48;

// Creates the view for the SPC progress bar.
std::unique_ptr<views::ProgressBar>
CreateSecurePaymentConfirmationProgressBarView();

// Creates the header view, which contains the icon and a progress bar. The icon
// covers the whole header view with the progress bar at the top of the header.
// +------------------------------------------+
// |===============progress bar===============|
// |                                          |
// |                   icon                   |
// +------------------------------------------+
std::unique_ptr<views::View> CreateSecurePaymentConfirmationHeaderView(
    int progress_bar_id,
    int header_icon_id,
    bool use_cart_image = false);

// Creates the label view for the SPC title text.
std::unique_ptr<views::Label> CreateSecurePaymentConfirmationTitleLabel(
    const std::u16string& title);

/// Creates the image view for the SPC instrument icon.
std::unique_ptr<views::ImageView>
CreateSecurePaymentConfirmationInstrumentIconView(const gfx::ImageSkia& bitmap);

// Formats the merchant label by combining the name and origin for display.
std::u16string FormatMerchantLabel(
    const absl::optional<std::u16string>& merchant_name,
    const absl::optional<std::u16string>& merchant_origin);

// Creates a label with a link to allow the user to delete their payment related
// data from the relying party.
std::unique_ptr<views::StyledLabel> CreateSecurePaymentConfirmationOptOutView(
    const std::u16string& relying_party_id,
    const std::u16string& opt_out_label,
    const std::u16string& opt_out_link_label,
    base::RepeatingClosure on_click);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_
