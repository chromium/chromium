// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_PASSWORD_CHECKUP_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_PASSWORD_CHECKUP_PROMO_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class PrefService;
#include "chrome/browser/ui/webui/password_manager/notification_card.h"

namespace extensions {
class PasswordsPrivateDelegate;
}  // namespace extensions

// Password checkup notification card. Despite other notification cards this one
// should be shown regularly but not more often than
// kPasswordCheckupPromoPeriod.
class PasswordCheckupPromo
    : public password_manager::PasswordNotificationCardBase {
 public:
  PasswordCheckupPromo(PrefService* prefs,
                       extensions::PasswordsPrivateDelegate* delegate);
  ~PasswordCheckupPromo() override;

 private:
  // PasswordNotificationCardBase implementation.
  std::string GetCardID() const override;
  password_manager::NotificationCardType GetNotificationCardType()
      const override;
  bool ShouldShowCard(const password_manager::NotificationCardPrefState&
                          pref_state) const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;

  raw_ptr<PrefService> prefs_ = nullptr;
  base::WeakPtr<extensions::PasswordsPrivateDelegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_PASSWORD_CHECKUP_PROMO_H_
