// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_PASSWORD_MANAGER_SHORTCUT_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_PASSWORD_MANAGER_SHORTCUT_PROMO_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/password_manager/notification_card.h"

class Profile;

// Promo card to create shortcut to the Password Manager.
class PasswordManagerShortcutPromo
    : public password_manager::PasswordNotificationCardBase {
 public:
  explicit PasswordManagerShortcutPromo(Profile* profile);

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

  bool is_shortcut_installed_ = false;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_PASSWORD_MANAGER_SHORTCUT_PROMO_H_
