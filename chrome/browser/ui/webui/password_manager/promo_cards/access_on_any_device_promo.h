// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_ACCESS_ON_ANY_DEVICE_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_ACCESS_ON_ANY_DEVICE_PROMO_H_

#include "chrome/browser/ui/webui/password_manager/promo_card.h"

// Promo card to communicate how to use Password Manager on Android and iOS.
class AccessOnAnyDevicePromo : public password_manager::PasswordPromoCardBase {
 public:
  explicit AccessOnAnyDevicePromo(PrefService* prefs);

 private:
  // PasswordPromoCardBase implementation.
  std::string GetPromoID() const override;
  password_manager::PromoCardType GetPromoCardType() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_ACCESS_ON_ANY_DEVICE_PROMO_H_
