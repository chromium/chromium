// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_

#include <memory>
#include <string>


class SkBitmap;

namespace views {
class Label;
class ProgressBar;
class View;
class ImageView;
}  // namespace views

namespace payments {

// Height of the header icon.
constexpr int kHeaderIconHeight = 148;

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

// Extra inset between the body content and the dialog buttons.
constexpr int kBodyExtraInset = 16;

// Size of the instrument icon.
constexpr int kInstrumentIconWidth = 32;
constexpr int kInstrumentIconHeight = 20;

// Height of each payment information row.
constexpr int kPaymentInfoRowHeight = 48;

int GetSecurePaymentConfirmationHeaderWidth();

// Creates the view for the SPC fingerprint header icon.
std::unique_ptr<views::View> CreateSecurePaymentConfirmationIconView(
    bool dark_mode);

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
    bool dark_mode,
    int progress_bar_id,
    int header_icon_id);

// Creates the label view for the SPC title text.
std::unique_ptr<views::Label> CreateSecurePaymentConfirmationTitleLabel(
    const std::u16string& title);

/// Creates the image view for the SPC instrument icon.
std::unique_ptr<views::ImageView>
CreateSecurePaymentConfirmationInstrumentIconView(const SkBitmap& bitmap);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_
