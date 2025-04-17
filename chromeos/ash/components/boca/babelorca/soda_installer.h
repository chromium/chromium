// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_SODA_INSTALLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_SODA_INSTALLER_H_

#include <queue>

#include "base/functional/callback_forward.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"

namespace ash::babelorca {

class SodaInstaller : public speech::SodaInstaller::Observer {
 public:
  enum InstallationStatus {
    kUninstalled,
    kReady,
    kInstalling,
    kInstallationFailure,
    kLanguageUnavailable,
  };

  using AvailabilityCallback =
      base::OnceCallback<void(InstallationStatus status)>;

  SodaInstaller(PrefService* global_prefs,
                PrefService* profile_prefs,
                const std::string language);
  ~SodaInstaller() override;
  SodaInstaller(const SodaInstaller&) = delete;
  SodaInstaller operator=(const SodaInstaller&) = delete;

  InstallationStatus GetStatus();
  void InstallSoda(AvailabilityCallback callback);

  // speech::SodaInstaller::Observer
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;

 private:
  void FlushCallbacks(InstallationStatus result);

  bool ValidLanguage();
  bool IsAlreadyInstalled();

  InstallationStatus status_ = InstallationStatus::kUninstalled;
  std::queue<AvailabilityCallback> callbacks_;
  const std::string language_;
  raw_ptr<PrefService> global_prefs_;
  raw_ptr<PrefService> profile_prefs_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_SODA_INSTALLER_H_
