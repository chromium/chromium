// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_SCREENLOCK_REAUTH_PROMO_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_SCREENLOCK_REAUTH_PROMO_H_

#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/password_manager/promo_card.h"

// Promo card to promote enabling reauth before accessing saved passwords.
class ScreenlockReauthPromo : public password_manager::PasswordPromoCardBase {
 public:
  explicit ScreenlockReauthPromo(Profile* profile);
  ScreenlockReauthPromo(
      Profile* profile,
      std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator);

  ~ScreenlockReauthPromo() override;

 private:
  // PasswordPromoCardBase implementation.
  std::string GetPromoID() const override;
  password_manager::PromoCardType GetPromoCardType() const override;
  bool ShouldShowPromo() const override;
  std::u16string GetTitle() const override;
  std::u16string GetDescription() const override;
  std::u16string GetActionButtonText() const override;

  raw_ptr<Profile> profile_;
  std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_SCREENLOCK_REAUTH_PROMO_H_
