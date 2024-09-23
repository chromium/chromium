// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_SODA_INSTALLER_IMPL_CHROMEOS_H_
#define COMPONENTS_SODA_SODA_INSTALLER_IMPL_CHROMEOS_H_

#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "components/soda/soda_installer.h"

class PrefService;

namespace speech {

// Installer of SODA (Speech On-Device API) for the Live Caption feature on
// ChromeOS.
class COMPONENT_EXPORT(SODA_INSTALLER) SodaInstallerImplChromeOS
    : public SodaInstaller {
 public:
  SodaInstallerImplChromeOS();
  ~SodaInstallerImplChromeOS() override;
  SodaInstallerImplChromeOS(const SodaInstallerImplChromeOS&) = delete;
  SodaInstallerImplChromeOS& operator=(const SodaInstallerImplChromeOS&) =
      delete;

  // Where the SODA DLC was installed. Cached on completed installation.
  // Empty if SODA DLC not installed yet.
  base::FilePath GetSodaBinaryPath() const override;

  // Where the SODA language pack DLC was installed for a given language.
  // Cached on completed installation. Empty if not installed yet.
  base::FilePath GetLanguagePath(const std::string& language) const override;

  // SodaInstaller:
  void InstallLanguage(const std::string& language,
                       PrefService* global_prefs) override;
  void UninstallLanguage(const std::string& language,
                         PrefService* global_prefs) override;
  std::vector<std::string> GetAvailableLanguages() const override;
  std::vector<std::string> GetLiveCaptionEnabledLanguages() const override;
  std::string GetLanguageDlcNameForLocale(
      const std::string& locale) const override;

 private:
  // SodaInstaller:
  void InstallSoda(PrefService* global_prefs) override;
  // Here "uninstall" is used in the DLC sense of the term: Uninstallation will
  // disable a DLC but not immediately remove it from disk.
  // Once a refcount to the DLC reaches 0 (meaning all profiles which had it
  // installed have called to uninstall it), the DLC will remain in cache; if it
  // is then not installed within a (DLC-service-defined) window of time, the
  // DLC is automatically purged from disk.
  void UninstallSoda(PrefService* global_prefs) override;

  void SetSodaBinaryPath(base::FilePath new_path);
  void SetLanguagePath(const LanguageCode language, base::FilePath new_path);

  // Initializes language and installs the per-language components.
  void InitLanguages(PrefService* profile_prefs,
                     PrefService* global_prefs) override;

  // These functions are the InstallCallbacks for DlcserviceClient::Install().
  void OnSodaInstalled(
      const base::Time start_time,
      const ash::DlcserviceClient::InstallResult& install_result);
  void OnLanguageInstalled(
      const LanguageCode language_code,
      const std::string language_name,
      const base::Time start_time,
      const ash::DlcserviceClient::InstallResult& install_result);

  // These functions are the ProgressCallbacks for DlcserviceClient::Install().
  void OnSodaProgress(double progress);
  void OnLanguageProgress(const LanguageCode language_code, double progress);

  void OnSodaCombinedProgress();

  // This is the UninstallCallback for DlcserviceClient::Uninstall().
  void OnDlcUninstalled(std::string_view dlc_id, std::string_view err);

  double soda_progress_ = 0.0;

  base::FilePath soda_lib_path_;

  base::flat_map<LanguageCode, base::FilePath> installed_language_paths_;

  struct LanguageInfo {
    std::string dlc_name;
    LanguageCode language_code;
  };
  base::flat_map<std::string, LanguageInfo> ConstructAvailableLanguages() const;

  base::flat_map<std::string, LanguageInfo> available_languages_;
};

}  // namespace speech

#endif  // COMPONENTS_SODA_SODA_INSTALLER_IMPL_CHROMEOS_H_
