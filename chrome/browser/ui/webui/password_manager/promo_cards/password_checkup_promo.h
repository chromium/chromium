// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_PASSWORD_CHECKUP_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_PASSWORD_CHECKUP_PROMO_H_

#include "chrome/browser/ui/webui/password_manager/promo_card.h"

namespace extensions {
class PasswordsPrivateDelegate;
}  // namespace extensions

// Password checkup promo card. Despite other promo cards this one should be
// shown regularly but not more often than kPasswordCheckupPromoPeriod.
class PasswordCheckupPromo : public password_manager::PasswordPromoCardBase {
 public:
  PasswordCheckupPromo(PrefService* prefs,
                       extensions::PasswordsPrivateDelegate* delegate);
  ~PasswordCheckupPromo() override;

 private:
  // PasswordPromoCardBase implementation.
  std::string GetPromoID() const override;
  password_manager::PromoCardType GetPromoCardType() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;

  base::WeakPtr<extensions::PasswordsPrivateDelegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_PASSWORD_CHECKUP_PROMO_H_
