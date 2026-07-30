// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_MOVE_PASSWORDS_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_MOVE_PASSWORDS_PROMO_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/password_manager/notification_card.h"

namespace extensions {
class PasswordsPrivateDelegate;
}

// Promo card to communicate that there are passwords saved only on this device.
class MovePasswordsPromo
    : public password_manager::PasswordNotificationCardBase {
 public:
  explicit MovePasswordsPromo(Profile* profile,
                              extensions::PasswordsPrivateDelegate* delegate);
  ~MovePasswordsPromo() override;

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

  raw_ptr<Profile> profile_;
  base::WeakPtr<extensions::PasswordsPrivateDelegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_MOVE_PASSWORDS_PROMO_H_
