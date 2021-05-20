// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_SODA_INSTALLER_H_
#define COMPONENTS_SODA_SODA_INSTALLER_H_

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/soda/constants.h"

class PrefService;

namespace speech {

// Installer of SODA (Speech On-Device API). This is a singleton because there
// is only one installation of SODA per device.
class SodaInstaller {
 public:
  // Observer of the SODA (Speech On-Device API) installation.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the SODA binary component and at least one language pack is
    // installed.
    virtual void OnSodaInstalled() = 0;

    // Called when a SODA language pack component is installed.
    virtual void OnSodaLanguagePackInstalled(LanguageCode language_code) = 0;

    // Called if there is an error in the SODA binary or language pack
    // installation.
    virtual void OnSodaError() = 0;

    // Called if there is an error in a SODA language pack installation.
    virtual void OnSodaLanguagePackError(
        speech::LanguageCode language_code) = 0;

    // Called during the SODA installation. Progress is the weighted average of
    // the download percentage of the SODA binary and at least one language
    // pack.
    virtual void OnSodaProgress(int combined_progress) = 0;

    // Called during the SODA installation. Progress is the download percentage
    // out of 100.
    virtual void OnSodaLanguagePackProgress(int language_progress,
                                            LanguageCode language_code) = 0;
  };

  SodaInstaller();
  virtual ~SodaInstaller();
  SodaInstaller(const SodaInstaller&) = delete;
  SodaInstaller& operator=(const SodaInstaller&) = delete;

  // Implemented in the platform-specific subclass to get the SodaInstaller
  // instance.
  static SodaInstaller* GetInstance();

  // Registers user preferences related to the Speech On-Device API (SODA)
  // component.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Initialize SODA if any SODA-utilising feature is enabled. Intended to be
  // called during embedder startup. Checks whether SODA is due for
  // uninstallation, and if so, triggers uninstallation.
  void Init(PrefService* profile_prefs, PrefService* global_prefs);

  // Schedules SODA for uninstallation if no SODA client features are
  // currently enabled. Should be called when client features using SODA are
  // disabled.
  void SetUninstallTimer(PrefService* profile_prefs, PrefService* global_prefs);

  // Gets the directory path of the installed SODA lib bundle, or an empty path
  // if not installed. Currently Chrome OS only, returns empty path on other
  // platforms.
  virtual base::FilePath GetSodaBinaryPath() const = 0;

  // Gets the directory path of the installed SODA language bundle, or an empty
  // path if not installed. Currently Chrome OS only, returns empty path on
  // other platforms.
  virtual base::FilePath GetLanguagePath() const = 0;

  // Installs the user-selected SODA language model. Called by
  // LiveCaptionController when the kLiveCaptionEnabled or
  // kLiveCaptionLanguageCode preferences change. `language` is a localized
  // language e.g. "en-US". `global_prefs` is passed as part of component
  // registration for the non-ChromeOS implementation.
  virtual void InstallLanguage(const std::string& language,
                               PrefService* global_prefs) = 0;

  // Returns whether or not SODA is installed on this device. Will return a
  // stale value until InstallSoda() and InstallLanguage() have run and
  // asynchronously returned an answer.
  virtual bool IsSodaInstalled() const = 0;

  // Returns whether or not the language pack for a given language or locale
  // code is installed.
  virtual bool IsLanguageInstalled(
      const std::string& locale_or_language) const = 0;

  // Adds an observer to the observer list.
  void AddObserver(Observer* observer);

  // Removes an observer from the observer list.
  void RemoveObserver(Observer* observer);

  void NotifySodaInstalledForTesting();

 protected:
  // Registers the preference tracking the installed SODA language packs.
  static void RegisterRegisteredLanguagePackPref(PrefRegistrySimple* registry);

  // Installs the SODA binary. `global_prefs` is passed as part of component
  // registration for the non-chromeos implementation.
  virtual void InstallSoda(PrefService* global_prefs) = 0;

  // Uninstalls SODA and associated language model(s). On some platforms, disc
  // space may not be freed immediately.
  virtual void UninstallSoda(PrefService* global_prefs) = 0;

  // Notifies the observers that the installation of the SODA binary and at
  // least one language pack has completed.
  void NotifyOnSodaInstalled();

  // Notifies the observers that a SODA language pack installation has
  // completed.
  void NotifyOnSodaLanguagePackInstalled(speech::LanguageCode language_code);

  // Notifies the observers that there is an error in the SODA binary
  // installation.
  void NotifyOnSodaError();

  // Notifies the observers that there is an error in a SODA language pack
  // installation.
  void NotifyOnSodaLanguagePackError(speech::LanguageCode language_code);

  // Notifies the observers of the combined progress as the SODA binary and
  // language pack are installed. Progress is the download percentage out of
  // 100.
  void NotifyOnSodaProgress(int combined_progress);

  // Notifies the observers of the progress percentage the SODA language pack is
  // installed. Progress is the download percentage out of 100.
  void NotifyOnSodaLanguagePackProgress(int language_progress,
                                        LanguageCode language_code);

  // Registers a language pack by adding it to the preference tracking the
  // installed SODA language packs.
  void RegisterLanguage(const std::string& language, PrefService* global_prefs);

  // Unregisters all language packs by clearing the preference tracking the
  // installed SODA language packs.
  void UnregisterLanguages(PrefService* global_prefs);

  base::ObserverList<Observer> observers_;
  bool soda_binary_installed_ = false;
  bool language_installed_ = false;
  bool soda_installer_initialized_ = false;

 private:
  // Any new feature using SODA should add its pref here.
  bool IsAnyFeatureUsingSodaEnabled(PrefService* prefs);
};

}  // namespace speech

#endif  // COMPONENTS_SODA_SODA_INSTALLER_H_
