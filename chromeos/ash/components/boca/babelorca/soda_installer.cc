// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/soda_installer.h"

#include <algorithm>

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

SodaInstaller::InstallationStatus SodaInstaller::GetStatus() {
  if (!ValidLanguage()) {
    status_ = InstallationStatus::kLanguageUnavailable;
    return status_;
  }

  if (IsAlreadyInstalled()) {
    status_ = InstallationStatus::kReady;
    return status_;
  }

  return status_;
}

void SodaInstaller::InstallSoda(AvailabilityCallback callback) {
  if (!ValidLanguage()) {
    status_ = InstallationStatus::kLanguageUnavailable;
    std::move(callback).Run(InstallationStatus::kLanguageUnavailable);
    return;
  }

  if (IsAlreadyInstalled()) {
    status_ = InstallationStatus::kReady;
    std::move(callback).Run(InstallationStatus::kReady);
    return;
  }

  callbacks_.push(std::move(callback));

  if (status_ == InstallationStatus::kInstalling) {
    return;
  }
  status_ = InstallationStatus::kInstalling;

  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();

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
  if (callbacks_.empty() ||
      language_code != speech::GetLanguageCode(language_)) {
    return;
  }

  status_ = InstallationStatus::kReady;
  FlushCallbacks(InstallationStatus::kReady);

  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
}

void SodaInstaller::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  if (callbacks_.empty()) {
    return;
  }

  // If the language code is kNone then the binary failed to install.
  if (language_code == speech::GetLanguageCode(language_) ||
      language_code == speech::LanguageCode::kNone) {
    status_ = InstallationStatus::kInstallationFailure;
    FlushCallbacks(InstallationStatus::kInstallationFailure);
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  }
}

// TODO(384026579): The bubble seems to be able to display SODA install
// progress, dispatch these updates to the bubble?
void SodaInstaller::OnSodaProgress(speech::LanguageCode language_code,
                                   int progress) {}

void SodaInstaller::FlushCallbacks(InstallationStatus status) {
  while (!callbacks_.empty()) {
    std::move(callbacks_.front()).Run(status);
    callbacks_.pop();
  }
}

bool SodaInstaller::ValidLanguage() {
  std::vector<std::string> languages =
      speech::SodaInstaller::GetInstance()->GetAvailableLanguages();

  return std::find(languages.begin(), languages.end(), language_) !=
         languages.end();
}

bool SodaInstaller::IsAlreadyInstalled() {
  return speech::SodaInstaller::GetInstance()->IsSodaInstalled(
      speech::GetLanguageCode(language_));
}

}  // namespace ash::babelorca
