// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_SODA_INSTALLATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_SODA_INSTALLATION_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/locale_update_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/soda/soda_installer.h"

namespace ash {
class ProjectorAppClient;
class ProjectorController;
}  // namespace ash

namespace speech {
enum class LanguageCode;
}  // namespace speech

// Class owned by ProjectorClientImpl used to control the installation of
// SODA and the language pack requested by the user. The main purpose
// of this class is to observe SODA installer and notify Projector App and
// Projector Controller on installation status.
class ProjectorSodaInstallationController
    : public speech::SodaInstaller::Observer,
      public ash::LocaleChangeObserver {
 public:
  ProjectorSodaInstallationController(ash::ProjectorAppClient* app_client,
                                      ash::ProjectorController* controller);
  ProjectorSodaInstallationController(
      const ProjectorSodaInstallationController&) = delete;
  ProjectorSodaInstallationController& operator=(
      const ProjectorSodaInstallationController&) = delete;

  ~ProjectorSodaInstallationController() override;

  // Installs the SODA binary and the the corresponding language if it is not
  // present.
  static void InstallSoda(const std::string& language);

  // Checks if the device is eligible to install SODA and language pack for the
  // `language` provided.
  static bool ShouldDownloadSoda(speech::LanguageCode language);

  // Checks if SODA binary and the requested `language` is downloaded and
  // available on device.
  static bool IsSodaAvailable(speech::LanguageCode language);

 protected:
  // speech::SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;

  // ash::LocaleChangeObserver:
  void OnLocaleChanged() override;

  const raw_ptr<ash::ProjectorAppClient> app_client_;
  const raw_ptr<ash::ProjectorController> projector_controller_;

 private:
  base::ScopedObservation<speech::SodaInstaller,
                          speech::SodaInstaller::Observer>
      soda_installer_observation_{this};

  base::ScopedObservation<ash::LocaleUpdateController,
                          ash::LocaleChangeObserver>
      locale_change_observation_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_SODA_INSTALLATION_CONTROLLER_H_
