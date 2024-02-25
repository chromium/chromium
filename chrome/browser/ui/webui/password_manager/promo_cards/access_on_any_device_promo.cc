// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/access_on_any_device_promo.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

extern const char16_t kGetStartedOnAndroid[] =
    u"https://support.google.com/chrome/?p=gpm_desktop_promo_android";
extern const char16_t kGetStartedOnIOS[] =
    u"https://support.google.com/chrome/?p=gpm_desktop_promo_ios";
constexpr char kAccessOnAnyDevicePromoId[] = "access_on_any_device_promo";

AccessOnAnyDevicePromo::AccessOnAnyDevicePromo(PrefService* prefs)
    : password_manager::PasswordPromoCardBase(kAccessOnAnyDevicePromoId,
                                              prefs) {}

std::string AccessOnAnyDevicePromo::GetPromoID() const {
  return kAccessOnAnyDevicePromoId;
}

password_manager::PromoCardType AccessOnAnyDevicePromo::GetPromoCardType()
    const {
  return password_manager::PromoCardType::kAccessOnAnyDevice;
}

bool AccessOnAnyDevicePromo::ShouldShowPromo() const {
  return !was_dismissed_ &&
         number_of_times_shown_ < PasswordPromoCardBase::kPromoDisplayLimit;
}

std::u16string AccessOnAnyDevicePromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_ANY_DEVICE_PROMO_CARD_TITLE);
}

std::u16string AccessOnAnyDevicePromo::GetDescription() const {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_UI_ANY_DEVICE_PROMO_CARD_DESCRIPTION,
      kGetStartedOnAndroid, kGetStartedOnIOS);
}
