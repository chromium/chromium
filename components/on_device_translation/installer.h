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
  };

  // Returns the singleton instance that implements
  // `OnDeviceTranslationInstaller`.
  static OnDeviceTranslationInstaller* GetInstance();

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
  // `on_ready_callback` is called.
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
