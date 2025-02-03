// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/soda_installer.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"

namespace ash::babelorca {

SodaInstaller::SodaInstaller(PrefService* global_prefs,
                             PrefService* profile_prefs,
                             const std::string language)
    : language_(std::move(language)),
      global_prefs_(global_prefs),
      profile_prefs_(profile_prefs) {}
SodaInstaller::~SodaInstaller() {
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
}
void SodaInstaller::GetAvailabilityOrInstall(AvailabilityCallback callback) {
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  speech::LanguageCode language_code =
      speech::GetLanguageCode(std::string(language_));

  if (soda_installer->IsSodaInstalled(language_code)) {
    std::move(callback).Run(true);
    return;
  }

  callback_ = std::move(callback);
  soda_installer->AddObserver(this);

  // if no other SODA features are currently active
  // then we need to Init SODA.  This will install the
  // binary.
  soda_installer->Init(profile_prefs_, global_prefs_);
  // By default the live caption language is installed
  // but the GetUserMicrophoneCaptionLanguage may be
  // different, so to ensure the pack is available
  // we install the specific language here.
  soda_installer->InstallLanguage(language_, global_prefs_);
}

void SodaInstaller::OnSodaInstalled(speech::LanguageCode language_code) {
  // This event is triggered whenever any language pack or the SODA binary is
  // installed, but we should only begin speech recognition when the language
  // pack associated with BabelOrca is installed.
  if (callback_.is_null() ||
      language_code != speech::GetLanguageCode(language_)) {
    return;
  }

  std::move(callback_).Run(true);
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
}

void SodaInstaller::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  if (callback_.is_null()) {
    return;
  }

  // If the language code is kNone then the binary failed to install.
  if (language_code == speech::GetLanguageCode(language_) ||
      language_code == speech::LanguageCode::kNone) {
    std::move(callback_).Run(false);
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  }
}

// TODO(384026579): The bubble seems to be able to display SODA install
// progress, dispatch these updates to the bubble?
void SodaInstaller::OnSodaProgress(speech::LanguageCode language_code,
                                   int progress) {}

}  // namespace ash::babelorca
