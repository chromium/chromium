// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_PASSKEY_UNLOCK_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_PASSKEY_UNLOCK_PROMO_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/password_manager/notification_card.h"

namespace webauthn {
class PasskeyUnlockManager;
}

// Promo card prompting the user to verify identity to use passkeys on this
// device.
class PasskeyUnlockPromo
    : public password_manager::PasswordNotificationCardBase {
 public:
  explicit PasskeyUnlockPromo(
      webauthn::PasskeyUnlockManager* passkey_unlock_manager);
  ~PasskeyUnlockPromo() override;

  // PasswordNotificationCardBase implementation.
  std::string GetCardID() const override;
  password_manager::NotificationCardType GetNotificationCardType()
      const override;
  bool ShouldShowCard(const password_manager::NotificationCardPrefState&
                          pref_state) const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;

 private:
  raw_ptr<webauthn::PasskeyUnlockManager> passkey_unlock_manager_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_PASSKEY_UNLOCK_PROMO_H_
