// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_INSTALLER_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_INSTALLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/prefs/pref_service.h"

class PrefService;

namespace on_device_translation {

class FakeOnDeviceTranslationInstaller : public OnDeviceTranslationInstaller {
 public:
  ~FakeOnDeviceTranslationInstaller() override;
  FakeOnDeviceTranslationInstaller();

  bool IsInit(PrefService* prefs) const override;
  std::set<LanguagePackKey> RegisteredLanguagePacks(
      PrefService* prefs) const override;
  std::set<LanguagePackKey> InstalledLanguagePacks(
      PrefService* prefs) const override;
  void Init(PrefService* pref_service,
            base::RepeatingClosure on_ready_callback) override;
  // Forces initialization to happen right away.
  void InitNow(base::RepeatingClosure on_ready_callback);
  bool InstallLanguagePack(LanguagePackKey language_pack,
                           PrefService* pref_service) override;
  // Forces installation to happen right away.
  void InstallLanguagePackNow(LanguagePackKey language_pack);
  bool UnInstallLanguagePack(LanguagePackKey language_pack,
                             PrefService* pref_service) override;
  void AddOserver(Observer* observer) override;

 private:
  bool is_init_ = false;
  std::set<LanguagePackKey> installed_lang_packs_;
  std::set<LanguagePackKey> registered_lang_packs_;
  base::ObserverList<OnDeviceTranslationInstaller::Observer> observers_;
  base::WeakPtrFactory<FakeOnDeviceTranslationInstaller> weak_ptr_factory_{
      this};
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_INSTALLER_H_
