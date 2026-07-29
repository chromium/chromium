// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_ACCESS_ON_ANY_DEVICE_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_ACCESS_ON_ANY_DEVICE_PROMO_H_

#include "chrome/browser/ui/webui/password_manager/notification_card.h"

// Promo card to communicate how to use Password Manager on Android and iOS.
class AccessOnAnyDevicePromo
    : public password_manager::PasswordNotificationCardBase {
 public:
  explicit AccessOnAnyDevicePromo(PrefService* prefs);

 private:
  // PasswordNotificationCardBase implementation.
  std::string GetCardID() const override;
  password_manager::NotificationCardType GetNotificationCardType()
      const override;
  bool ShouldShowCard() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_ACCESS_ON_ANY_DEVICE_PROMO_H_
