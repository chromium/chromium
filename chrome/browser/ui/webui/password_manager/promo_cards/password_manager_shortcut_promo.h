// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_PASSWORD_MANAGER_SHORTCUT_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_PASSWORD_MANAGER_SHORTCUT_PROMO_H_

#include "chrome/browser/ui/webui/password_manager/promo_card.h"

class Profile;

// Promo card to create shortcut to the Password Manager.
class PasswordManagerShortcutPromo
    : public password_manager::PasswordPromoCardBase {
 public:
  explicit PasswordManagerShortcutPromo(Profile* profile);

 private:
  // PasswordPromoCardBase implementation.
  std::string GetPromoID() const override;
  password_manager::PromoCardType GetPromoCardType() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;

  bool is_shortcut_installed_ = false;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_PASSWORD_MANAGER_SHORTCUT_PROMO_H_
