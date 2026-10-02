// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_ACCOUNT_AWARE_PASSWORD_SIGNIN_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_ACCOUNT_AWARE_PASSWORD_SIGNIN_PROMO_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/password_manager/notification_card.h"
#include "components/signin/public/identity_manager/account_info.h"

class Profile;

// Account-aware sign-in promo card shown in the Password Manager to web-only
// signed-in users whose preferred promo account has password preview data.
class AccountAwarePasswordSigninPromo
    : public password_manager::PasswordNotificationCardBase {
 public:
  explicit AccountAwarePasswordSigninPromo(Profile* profile);
  AccountAwarePasswordSigninPromo(const AccountAwarePasswordSigninPromo&) =
      delete;
  AccountAwarePasswordSigninPromo& operator=(
      const AccountAwarePasswordSigninPromo&) = delete;
  ~AccountAwarePasswordSigninPromo() override;

  // password_manager::PasswordNotificationCardBase:
  std::string GetCardID() const override;
  password_manager::NotificationCardType GetNotificationCardType()
      const override;
  password_manager::NotificationSeverity GetNotificationSeverity()
      const override;
  bool ShouldShowCard(const password_manager::NotificationCardPrefState&
                          pref_state) const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;
  std::string GetActionButtonAvatarUrl() const override;

 private:
  AccountInfo GetAccountForPromo() const;

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_ACCOUNT_AWARE_PASSWORD_SIGNIN_PROMO_H_
