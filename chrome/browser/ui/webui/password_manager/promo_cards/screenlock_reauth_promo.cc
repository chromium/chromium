// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards/screenlock_reauth_promo.h"

#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kScreenlockReauthId[] = "screenlock_reauth_promo";
constexpr base::TimeDelta kScreenlockReauthPromoPeriod = base::Days(7);

std::unique_ptr<device_reauth::DeviceAuthenticator> GetDeviceAuthenticator(
    Profile* profile) {
  device_reauth::DeviceAuthParams params(
      base::Seconds(60), device_reauth::DeviceAuthSource::kPasswordManager);

  return ChromeDeviceAuthenticatorFactory::GetForProfile(profile, nullptr,
                                                         params);
}
}  // namespace

ScreenlockReauthPromo::ScreenlockReauthPromo(Profile* profile)
    : password_manager::PasswordPromoCardBase(kScreenlockReauthId,
                                              profile->GetPrefs()),
      profile_(profile) {
  device_authenticator_ = GetDeviceAuthenticator(profile_);
}

ScreenlockReauthPromo::ScreenlockReauthPromo(
    Profile* profile,
    std::unique_ptr<device_reauth::DeviceAuthenticator> device_authenticator)
    : password_manager::PasswordPromoCardBase(kScreenlockReauthId,
                                              profile->GetPrefs()),
      profile_(profile),
      device_authenticator_(std::move(device_authenticator)) {}

ScreenlockReauthPromo::~ScreenlockReauthPromo() = default;

std::string ScreenlockReauthPromo::GetPromoID() const {
  return kScreenlockReauthId;
}

password_manager::PromoCardType ScreenlockReauthPromo::GetPromoCardType()
    const {
  return password_manager::PromoCardType::kScreenlockReauth;
}

bool ScreenlockReauthPromo::ShouldShowPromo() const {
  const bool canDeviceUseBiometrics =
      device_authenticator_ &&
      device_authenticator_->CanAuthenticateWithBiometrics();
  const bool isReauthPrefDefault =
      profile_->GetPrefs()
          ->FindPreference(
              password_manager::prefs::kBiometricAuthenticationBeforeFilling)
          ->IsDefaultValue();

  // If promo card was dismissed or shown already for
  // `kPromoDisplayLimit` times, show it in a week next time.
  const bool should_suppress =
      was_dismissed_ ||
      number_of_times_shown_ >= PasswordPromoCardBase::kPromoDisplayLimit;
  const bool isNotBlocked =
      !should_suppress ||
      base::Time().Now() - last_time_shown_ > kScreenlockReauthPromoPeriod;

  return isNotBlocked && canDeviceUseBiometrics && isReauthPrefDefault &&
         base::FeatureList::IsEnabled(
             password_manager::features::kScreenlockReauthPromoCard);
}

std::u16string ScreenlockReauthPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_SCREENLOCK_REAUTH_PROMO_CARD_TITLE);
}

std::u16string ScreenlockReauthPromo::GetDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_SCREENLOCK_REAUTH_PROMO_CARD_DESCRIPTION);
}

std::u16string ScreenlockReauthPromo::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_SCREENLOCK_REAUTH_PROMO_CARD_ACTION);
}
