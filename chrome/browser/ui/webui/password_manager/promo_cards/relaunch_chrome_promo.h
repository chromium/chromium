// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_RELAUNCH_CHROME_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_RELAUNCH_CHROME_PROMO_H_

#include "chrome/browser/ui/webui/password_manager/promo_card.h"

// Promo card to communicate that there is an error with the Keychain.
class RelaunchChromePromo : public password_manager::PasswordPromoCardBase {
 public:
  explicit RelaunchChromePromo(PrefService* prefs);
  ~RelaunchChromePromo() override;

 private:
  // PasswordPromoCardBase implementation.
  std::string GetPromoID() const override;
  password_manager::PromoCardType GetPromoCardType() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_RELAUNCH_CHROME_PROMO_H_
