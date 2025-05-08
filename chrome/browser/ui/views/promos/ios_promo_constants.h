// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_

namespace IOSPromoConstants {

// iOS promo QR code URLs.
inline constexpr char kIOSPromoPasswordBubbleQRCodeURL[] =
    "https://www.google.com/chrome/go-mobile/"
    "?ios-campaign=desktop-chr-passwords&android-campaign=desktop-chr-"
    "passwords";
inline constexpr char kIOSPromoAddressBubbleQRCodeURL[] =
    "https://www.google.com/chrome/go-mobile/"
    "?ios-campaign=desktop-chr-address&android-campaign=desktop-chr-address";
inline constexpr char kIOSPromoPaymentBubbleQRCodeURL[] =
    "https://www.google.com/chrome/go-mobile/"
    "?ios-campaign=desktop-chr-payment&android-campaign=desktop-chr-payment";

// Size of the QR code image view including the quiet zone margin added by the
// QR code generator.
inline constexpr int kQrCodeImageSize = 90;

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
