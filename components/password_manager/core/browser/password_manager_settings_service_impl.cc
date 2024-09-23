// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_settings_service_impl.h"

#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

PasswordManagerSettingsServiceImpl::PasswordManagerSettingsServiceImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

bool PasswordManagerSettingsServiceImpl::IsSettingEnabled(
    PasswordManagerSetting setting) const {
  switch (setting) {
    case PasswordManagerSetting::kOfferToSavePasswords:
      return pref_service_->GetBoolean(
          password_manager::prefs::kCredentialsEnableService);
    case PasswordManagerSetting::kAutoSignIn:
      return pref_service_->GetBoolean(
          password_manager::prefs::kCredentialsEnableAutosignin);
    // This is Android specific specific setting which is available only with
    // GMS Core. PasswordManagerSettingsServiceImpl is instantiated only when
    // GMS Core is not in use, so always return false in this case.
    case PasswordManagerSetting::kBiometricReauthBeforePwdFilling:
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_CHROMEOS)
      return pref_service_->GetBoolean(
          password_manager::prefs::kBiometricAuthenticationBeforeFilling);
#else
      return false;
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  }
}

void PasswordManagerSettingsServiceImpl::RequestSettingsFromBackend() {
  // Settings fetching is needed only on Android when UPM is enabled. This
  // implementation of the settings service is only used when UPM is unusable,
  // so do nothing here.
}

void PasswordManagerSettingsServiceImpl::TurnOffAutoSignIn() {
  pref_service_->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
}

}  // namespace password_manager
