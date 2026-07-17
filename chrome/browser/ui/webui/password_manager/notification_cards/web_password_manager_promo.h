// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_WEB_PASSWORD_MANAGER_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_WEB_PASSWORD_MANAGER_PROMO_H_

#include "chrome/browser/ui/webui/password_manager/notification_card.h"

namespace syncer {
class SyncService;
}

// Promoting web version of Password Manager. Has a link to the website in the
// description.
class WebPasswordManagerPromo
    : public password_manager::PasswordNotificationCardBase {
 public:
  WebPasswordManagerPromo(PrefService* prefs,
                          const syncer::SyncService* sync_service);

 private:
  // PasswordNotificationCardBase implementation.
  std::string GetCardID() const override;
  password_manager::NotificationCardType GetNotificationCardType()
      const override;
  bool ShouldShowCard() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;

  bool sync_enabled_ = false;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_WEB_PASSWORD_MANAGER_PROMO_H_
