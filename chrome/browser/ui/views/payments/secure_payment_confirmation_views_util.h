// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_

#include <memory>

#include "base/strings/string16.h"

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

// Height of the progress bar at the top of the dialog.
constexpr int kProgressBarHeight = 4;

// Line height of the title text.
constexpr int kTitleLineHeight = 24;

// Line height of the description text.
constexpr int kDescriptionLineHeight = 20;

// Insets of the body content.
constexpr int kBodyInsets = 16;

// Extra inset between the body content and the dialog buttons.
constexpr int kBodyExtraInset = 24;

// Size of the instrument icon.
constexpr int kInstrumentIconWidth = 32;
constexpr int kInstrumentIconHeight = 20;

int GetSecurePaymentConfirmationHeaderWidth();

// Creates the view for the SPC fingerprint header icon.
std::unique_ptr<views::View> CreateSecurePaymentConfirmationHeaderView(
    bool dark_mode);

// Creates the view for the SPC progress bar.
std::unique_ptr<views::ProgressBar>
CreateSecurePaymentConfirmationProgressBarView();

// Creates the label view for the SPC title text.
std::unique_ptr<views::Label> CreateSecurePaymentConfirmationTitleLabel(
    const base::string16& title);

/// Creates the image view for the SPC instrument icon.
std::unique_ptr<views::ImageView>
CreateSecurePaymentConfirmationInstrumentIconView(const SkBitmap& bitmap);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SECURE_PAYMENT_CONFIRMATION_VIEWS_UTIL_H_
