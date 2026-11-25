// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_INSTALLER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_INSTALLER_H_

#include <set>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/on_device_translation/public/language_pack.h"

class PrefService;

namespace on_device_translation {

// Why the TranslateKit library or a language pack could not be installed.
enum class InstallError {
  // The download or the install failed.
  kFailed,
  // There is not enough free disk space for the download.
  kNotEnoughDiskSpace,

  // The device is offline, so the download didn't start.
  kOffline,
  // Data Saver is enabled, so the download didn't start.
  kDataSaverEnabled,
  // The connection is metered and metered downloads are blocked.
  kMeteredConnectionBlocked,
};

// This is a singleton interface, the actual instance should be obtained through
// the `GetInstance` static method. The implementations of this class should
// support the idea of registered language packs as well as installed language
// packs whereas the language packs are registered for installation which could
// take longer, i.e. a pack could be registered but not installed yet.
//
// This is a singleton because we want to have
// language installations to be per-device rather than per-profile.
class OnDeviceTranslationInstaller {
 public:
  OnDeviceTranslationInstaller();
  virtual ~OnDeviceTranslationInstaller();
  OnDeviceTranslationInstaller(const OnDeviceTranslationInstaller&) = delete;
  OnDeviceTranslationInstaller& operator=(const OnDeviceTranslationInstaller&) =
      delete;

  // Observer of the installation of language packs.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLanguagePackInstalled(const LanguagePackKey lang_pack) = 0;
    virtual void OnLanguagePackInstallationChanged(
        const LanguagePackKey lang_pack) = 0;
    virtual void OnInstallationChanged() = 0;
    // Called when the installation progress of a language pack changes.
    // `progress` is an integer percentage from 0 to 100.
    virtual void OnLanguagePackProgress(const LanguagePackKey lang_pack,
                                        int progress) {}
    // Called when an install of the TranslateKit library that `Init()`
    // started failed. A later `Init()` call retries the install.
    virtual void OnLibraryInstallFailed(InstallError error) {}
    // Called when a download of a language pack that `InstallLanguagePack()`
    // started failed. If the pack was already registered it stays registered,
    // while pre-download rejections (such as `kOffline`) leave an unregistered
    // pack unregistered. A later `InstallLanguagePack()` call retries the
    // download.
    virtual void OnLanguagePackInstallFailed(LanguagePackKey language_pack,
                                             InstallError error) {}
  };

  // Returns the singleton instance that implements
  // `OnDeviceTranslationInstaller`.
  static OnDeviceTranslationInstaller* GetInstance();

  // Returns whether the TranslateKit library can be installed and run on this
  // device. If not, translation is unavailable.
  virtual bool IsSupported() const = 0;
  // Returns whether the OnDeviceTranslation has completed initialization.
  virtual bool IsInit() const = 0;
  // Returns the set of registered language packs.
  virtual std::set<LanguagePackKey> RegisteredLanguagePacks() const = 0;
  // Returns the set of installed language packs.
  virtual std::set<LanguagePackKey> InstalledLanguagePacks() const = 0;

  // Returns the library installation path.
  virtual base::FilePath GetLibraryPath() const = 0;
  // Returns the installation directory for language pack.
  virtual base::FilePath GetLanguagePackPath(
      LanguagePackKey language_pack) const = 0;

  // Start initialization. When initialization is finished, the
  // `on_ready_callback` is called once. `on_ready_callback` may be null.
  virtual void Init(base::RepeatingClosure on_ready_callback) = 0;
  // Start installation of a language pack.
  virtual void InstallLanguagePack(LanguagePackKey language_pack) = 0;
  // Start uninstallation of a language pack.
  virtual void UnInstallLanguagePack(LanguagePackKey language_pack) = 0;

  // Subscribes a new observer to be notified of events.
  virtual void AddObserver(Observer* observer) = 0;
  // Unsubscribes an observer.
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_INSTALLER_H_
