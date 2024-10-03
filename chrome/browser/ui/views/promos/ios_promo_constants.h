// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_

namespace IOSPromoConstants {

// Size of the QR code image view including the quiet zone margin added by the
// QR code generator.
const int kQrCodeImageSize = 90;

// URL used for the QR code within the password bubble promo.
const char kPasswordBubbleQRCodeURL[] =
    "https://apps.apple.com/app/apple-store/"
    "id535886823?pt=9008&ct=desktop-chr-passwords&mt=8";

const char kAddressBubbleQRCodeURL[] =
    "https://apps.apple.com/app/apple-store/"
    "id535886823?pt=9008&ct=desktop-chr-address&mt=8";

const char kPaymentBubbleQRCodeURL[] =
    "https://apps.apple.com/app/apple-store/"
    "id535886823?pt=9008&ct=desktop-chr-payment&mt=8";

struct IOSPromoTypeConfigs {
  int bubble_title_id = -1;
  int bubble_subtitle_id = -1;
  int promo_title_id = -1;
  int promo_description_id = -1;
  int decline_button_text_id = -1;
  std::string promo_qr_code_url;
};

}  // namespace IOSPromoConstants

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_
