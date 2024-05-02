// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_

namespace IOSPromoConstants {

// Size of QR code image view including the Margin.
const int kQrCodeImageSize = 85;

// URL used for the QR code within the password bubble promo.
const char kPasswordBubbleQRCodeURL[] =
    "https://apps.apple.com/app/apple-store/"
    "id535886823?pt=9008&ct=desktop-chr-passwords&mt=8";

struct IOSPromoTypeConfigs {
  int kBubbleTitleID;
  int kBubbleSubtitleID;
  int kPromoTitleID;
  int kPromoDescriptionID;
  int kDeclineButtonTextID;
  std::string kPromoQRCodeURL;
};

}  // namespace IOSPromoConstants

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_
