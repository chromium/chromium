// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_SODA_INSTALLER_H_
#define COMPONENTS_SODA_SODA_INSTALLER_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/soda/constants.h"

class PrefService;

namespace speech {

// Installer of SODA (Speech On-Device API). This is a singleton because there
// is only one installation of SODA per device.
// SODA is not supported on some Chrome OS devices. Chrome OS callers should
// check if ash::features::kOnDeviceSpeechRecognition is enabled before
// trying to access the SodaInstaller instance.
class COMPONENT_EXPORT(SODA_INSTALLER) SodaInstaller {
 public:
  // Error codes passed to the observers.
  enum class ErrorCode {
    kUnspecifiedError,  // a default error.
    kNeedsReboot,       // libsoda requires an OS reboot on ChromeOS.
  };

  // Observer of the SODA (Speech On-Device API) installation.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the SODA binary component and the language pack for this
    // language code are installed.
    virtual void OnSodaInstalled(LanguageCode language_code) = 0;

    // Called if there is an error in the SODA installation. If the language
    // code is LanguageCode::kNone, the error is for the SODA binary; otherwise
    // it is for the language pack.
    virtual void OnSodaInstallError(LanguageCode language_code,
                                    ErrorCode error_code) = 0;

    // Called during the SODA installation. Progress is the weighted average of
    // the combined download percentage of the SODA binary and the language pack
    // for this language code.
    virtual void OnSodaProgress(LanguageCode language_code, int progress) = 0;
  };

  SodaInstaller();
  virtual ~SodaInstaller();
  SodaInstaller(const SodaInstaller&) = delete;
  SodaInstaller& operator=(const SodaInstaller&) = delete;

  static SodaInstaller* GetInstance();

  // Registers user preferences related to the Speech On-Device API (SODA)
  // component.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Initialize SODA if any SODA-utilising feature is enabled. Intended to be
  // called during embedder startup. Checks whether SODA is due for
  // uninstallation, and if so, triggers uninstallation.
  virtual void Init(PrefService* profile_prefs, PrefService* global_prefs);

  // Schedules SODA for uninstallation if no SODA client features are
  // currently enabled. Should be called when client features using SODA are
  // disabled.
  void SetUninstallTimer(PrefService* profile_prefs, PrefService* global_prefs);

  // Gets the directory path of the installed SODA lib bundle, or an empty path
  // if not installed. Currently Chrome OS only, returns empty path on other
  // platforms.
  virtual base::FilePath GetSodaBinaryPath() const = 0;

  // Gets the directory path of the installed SODA language bundle given a
  // localized language code in BCP-47 (e.g. "en-US"), or an empty
  // path if not installed. Currently Chrome OS only, returns empty path on
  // other platforms.
  virtual base::FilePath GetLanguagePath(const std::string& language) const = 0;

  // Installs the user-selected SODA language model. Called by
  // LiveCaptionController when the kLiveCaptionEnabled or
  // kLiveCaptionLanguageCode preferences change. `language` is a localized
  // language e.g. "en-US". `global_prefs` is passed as part of component
  // registration for the non-ChromeOS implementation.
  virtual void InstallLanguage(const std::string& language,
                               PrefService* global_prefs) = 0;

  // Uninstalls the language pack given a localized language code in BCP-47
  // (e.g. "en-US");
  virtual void UninstallLanguage(const std::string& language,
                                 PrefService* global_prefs) = 0;

  // Installs the SODA binary. `global_prefs` is passed as part of component
  // registration for the non-chromeos implementation.
  virtual void InstallSoda(PrefService* global_prefs) = 0;

  // Gets all installed and installable language codes supported by SODA
  // (in BCP-47 format).
  virtual std::vector<std::string> GetAvailableLanguages() const = 0;

  // Get the name of language DLC for a certain locale. This is currently only
  // being used in `SodaInstallerImplChromeOS`.
  virtual std::string GetLanguageDlcNameForLocale(
      const std::string& locale) const;

  // Returns whether or not SODA and the given language pack are installed on
  // this device. Will return a stale value until InstallSoda() and
  // InstallLanguage() have run and asynchronously returned an answer.
  bool IsSodaInstalled(LanguageCode language_code) const;

  // Returns whether or not the SODA binary is installed.
  bool IsSodaBinaryInstalled() const;

  // Adds an observer to the observer list.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

  // Method for checking in-progress downloads.
  bool IsSodaDownloading(LanguageCode language_code) const;

  // Returns the error encountered while installing soda for the language code
  // or soda binary.
  std::optional<ErrorCode> GetSodaInstallErrorCode(
      LanguageCode language_code) const;

  // TODO(crbug.com/40783492): Consider creating a MockSodaInstaller class that
  // implements these test-specific methods.
  void NeverDownloadSodaForTesting() {
    never_download_soda_for_testing_ = true;
  }

  // The soda binary is encoded as LanguageCode::kNone.
  void NotifySodaInstalledForTesting(
      LanguageCode language_code = LanguageCode::kNone);
  void NotifySodaErrorForTesting(
      LanguageCode language_code = LanguageCode::kNone,
      ErrorCode error = ErrorCode::kUnspecifiedError);
  void UninstallSodaForTesting();
  void NotifySodaProgressForTesting(
      int progress,
      LanguageCode language_code = LanguageCode::kNone);
  bool IsAnyLanguagePackInstalledForTesting() const;

  const std::set<LanguageCode> InstalledLanguages() const;
  // Gets a list of locales enabled by the Finch flag.
  virtual std::vector<std::string> GetLiveCaptionEnabledLanguages() const;

  // Registers a language pack by adding it to the preference tracking the
  // installed SODA language packs.
  void RegisterLanguage(const std::string& language, PrefService* global_prefs);

  void UnregisterLanguage(const std::string& language,
                          PrefService* global_prefs);

 protected:
  // Initializes language and installs the per-language components.
  virtual void InitLanguages(PrefService* profile_prefs,
                             PrefService* global_prefs);

  // Registers the preference tracking the installed SODA language packs.
  static void RegisterRegisteredLanguagePackPref(PrefRegistrySimple* registry);

  // Uninstalls SODA and associated language model(s). On some platforms, disc
  // space may not be freed immediately.
  virtual void UninstallSoda(PrefService* global_prefs) = 0;

  // Notifies the observers that the installation of the SODA binary and the
  // language pack for this language code has completed.
  void NotifyOnSodaInstalled(LanguageCode language_code);

  // Notifies the observers that there is an error in the SODA installation.
  // If the language code is LanguageCode::kNone, the error is for the SODA
  // binary; otherwise it is for the language pack.
  void NotifyOnSodaInstallError(LanguageCode language_code,
                                ErrorCode error_code);

  // Notifies the observers of the combined progress as the SODA binary and
  // language pack are installed. Progress is the download percentage out of
  // 100.
  void NotifyOnSodaProgress(LanguageCode language_code, int progress);

  // Unregisters all language packs by clearing the preference tracking the
  // installed SODA language packs.
  void UnregisterLanguages(PrefService* global_prefs);

  // Returns whether or not the language pack for a given language is
  // installed. The language should be localized in BCP-47, e.g. "en-US".
  bool IsLanguageInstalled(LanguageCode language_code) const;

  base::ObserverList<Observer> observers_;
  bool soda_binary_installed_ = false;
  bool soda_installer_initialized_ = false;
  bool is_soda_downloading_ = false;
  bool never_download_soda_for_testing_ = false;

  // Tracks all downloaded language packs.
  std::set<LanguageCode> installed_languages_;
  // Maps language codes to their install progress.
  base::flat_map<LanguageCode, double> language_pack_progress_;

  // The error state for the language code.
  base::flat_map<LanguageCode, ErrorCode> error_codes_;

 private:
  friend class SodaInstallerImplChromeOSTest;
  friend class SodaInstallerImplTest;
  // Any new feature using SODA should add its pref here.
  bool IsAnyFeatureUsingSodaEnabled(PrefService* prefs);
};

}  // namespace speech

#endif  // COMPONENTS_SODA_SODA_INSTALLER_H_
