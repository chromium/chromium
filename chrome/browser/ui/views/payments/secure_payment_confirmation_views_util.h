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
class Label;
class View;
class ImageView;
class StyledLabel;
}  // namespace views

namespace gfx {
class ImageSkia;
}

class SkBitmap;

namespace payments {

// Height of the header icons.
inline constexpr int kHeaderIconHeight = 148;
inline constexpr int kShoppingCartHeaderIconHeight = 114;

// Padding above the header icon.
inline constexpr int kHeaderIconTopPadding = 16;

// Line height of the title text.
inline constexpr int kTitleLineHeight = 24;

// Spacing between the icons in the inline title row.
inline constexpr int kInlineTitleRowHorizontalSpacing = 5;

// Required height of the icons in the inline title row.
inline constexpr int kInlineTitleIconHeight = 24;

// Max width of the icons in the inline title row.
inline constexpr int kInlineTitleMaxIconWidth = 40;

// Height of the separator between the icons in the inline title row.
inline constexpr int kInlineTitleIconSeparatorHeight = 20;

// Line height of the description text.
inline constexpr int kDescriptionLineHeight = 20;

// Insets of the secondary small text, e.g., the opt-out footer.
inline constexpr int kSecondarySmallTextInsets = 16;

// Extra inset between the body content and the dialog buttons.
inline constexpr int kBodyExtraInset = 16;

// Height of each payment information row.
inline constexpr int kPaymentInfoRowHeight = 48;

// Creates the header icon, showing either the transaction dialog logo or the no
// matching credentials dialog logo.
std::unique_ptr<views::View> CreateSecurePaymentConfirmationHeaderIcon(
    int header_icon_id,
    bool use_cart_image = false);

// Creates the 'inline' title view, where the network and issuer icons are
// placed beside the title text. Either or both of the network and issuer icons
// may be empty (i.e., drawsNothing returns true) in which case they are
// omitted from the output view.
//
// +------------------------------------------+
// | Title                        icon | icon |
// +------------------------------------------+
std::unique_ptr<views::View>
CreateSecurePaymentConfirmationInlineImageTitleView(
    std::unique_ptr<views::Label> title_text,
    const SkBitmap& network_icon,
    int network_icon_id,
    const SkBitmap& issuer_icon,
    int issuer_icon_id);

// Creates the label view for the SPC title text.
std::unique_ptr<views::Label> CreateSecurePaymentConfirmationTitleLabel(
    const std::u16string& title);

/// Creates an image view for an icon in the SPC transactions details table.
std::unique_ptr<views::ImageView> CreateSecurePaymentConfirmationIconView(
    const gfx::ImageSkia& bitmap);

// Formats the merchant label by combining the name and origin for display.
std::u16string FormatMerchantLabel(
    const std::optional<std::u16string>& merchant_name,
    const std::optional<std::u16string>& merchant_origin);

// Creates a label with a link to allow the user to delete their payment related
// data from the relying party.
std::unique_ptr<views::StyledLabel> CreateSecurePaymentConfirmationOptOutView(
    const std::u16string& relying_party_id,
    const std::u16string& opt_out_label,
    const std::u16string& opt_out_link_label,
    base::RepeatingClosure on_click);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_
