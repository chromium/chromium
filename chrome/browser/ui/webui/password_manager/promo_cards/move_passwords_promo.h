// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_MOVE_PASSWORDS_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_MOVE_PASSWORDS_PROMO_H_

#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/password_manager/promo_card.h"

// Promo card to communicate that there are passwords saved only on this device.
class MovePasswordsPromo : public password_manager::PasswordPromoCardBase {
 public:
  explicit MovePasswordsPromo(Profile* profile,
                              extensions::PasswordsPrivateDelegate* delegate);
  ~MovePasswordsPromo() override;

 private:
  // PasswordPromoCardBase implementation.
  std::string GetPromoID() const override;
  password_manager::PromoCardType GetPromoCardType() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;

  raw_ptr<Profile> profile_;
  base::WeakPtr<extensions::PasswordsPrivateDelegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_MOVE_PASSWORDS_PROMO_H_
