// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_

#include <string>

#include "ui/base/models/image_model.h"

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
inline constexpr char kIOSPromoLensBubbleQRCodeURL[] =
    "https://www.google.com/chrome/go-mobile/"
    "?ios-campaign=desktop-chr-lens-1&android-campaign=desktop-chr-lens-1";
inline constexpr char kIOSPromoESBBubbleQRCodeURL[] =
    "https://www.google.com/chrome/go-mobile/"
    "?ios-campaign=desktop-chr-esb-1&android-campaign=desktop-chr-esb-1";

// Size of the image view (QR code or otherwise) in the promos.
inline constexpr int kImageSize = 80;
// Element identifiers for the promo bubble. Used to update the promo bubble in
// place.
inline constexpr int kDescriptionLabelID = 1;
inline constexpr int kImageViewID = 2;

struct IOSPromoTypeConfigs {
  IOSPromoTypeConfigs();
  ~IOSPromoTypeConfigs();
  IOSPromoTypeConfigs(const IOSPromoTypeConfigs&);
  IOSPromoTypeConfigs& operator=(const IOSPromoTypeConfigs&);

  int bubble_title_id = -1;
  int bubble_subtitle_id = -1;
  int promo_title_id = -1;
  int promo_description_id = -1;
  int decline_button_text_id = -1;
  int accept_button_text_id = -1;
  int feature_name_id = -1;
  ui::ImageModel promo_image;
  bool with_header;
  std::string qr_code_url;
};

}  // namespace IOSPromoConstants

#endif  // CHROME_BROWSER_UI_VIEWS_PROMOS_IOS_PROMO_CONSTANTS_H_
