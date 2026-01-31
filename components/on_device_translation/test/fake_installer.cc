// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/test/fake_installer.h"

#include "base/test/gmock_callback_support.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/pref_service.h"

namespace on_device_translation {

FakeOnDeviceTranslationInstaller::~FakeOnDeviceTranslationInstaller() = default;
FakeOnDeviceTranslationInstaller::FakeOnDeviceTranslationInstaller() = default;

bool FakeOnDeviceTranslationInstaller::IsInit(PrefService* prefs) const {
  return is_init_;
}
std::set<LanguagePackKey>
FakeOnDeviceTranslationInstaller::RegisteredLanguagePacks(
    PrefService* prefs) const {
  return registered_lang_packs_;
}
std::set<LanguagePackKey>
FakeOnDeviceTranslationInstaller::InstalledLanguagePacks(
    PrefService* prefs) const {
  return installed_lang_packs_;
}

void FakeOnDeviceTranslationInstaller::Init(
    PrefService* pref_service,
    base::RepeatingClosure on_ready_callback) {
  if (is_init_) {
    on_ready_callback.Run();
    return;
  }
  // Schedule it to run on the next loop cycle
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeOnDeviceTranslationInstaller::InitNow,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(on_ready_callback)));
}
void FakeOnDeviceTranslationInstaller::InitNow(
    base::RepeatingClosure on_ready_callback) {
  is_init_ = true;
  if (on_ready_callback) {
    on_ready_callback.Run();
  }
}
bool FakeOnDeviceTranslationInstaller::InstallLanguagePack(
    LanguagePackKey language_pack,
    PrefService* pref_service) {
  registered_lang_packs_.insert(language_pack);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeOnDeviceTranslationInstaller::InstallLanguagePackNow,
                     weak_ptr_factory_.GetWeakPtr(), language_pack));
  return true;
}
void FakeOnDeviceTranslationInstaller::InstallLanguagePackNow(
    LanguagePackKey language_pack) {
  registered_lang_packs_.insert(language_pack);
  installed_lang_packs_.insert(language_pack);
  for (Observer& observer : observers_) {
    observer.OnLanguagePackInstalled(language_pack);
  }
}
bool FakeOnDeviceTranslationInstaller::UnInstallLanguagePack(
    LanguagePackKey language_pack,
    PrefService* pref_service) {
  registered_lang_packs_.erase(language_pack);
  installed_lang_packs_.erase(language_pack);
  return true;
}

void FakeOnDeviceTranslationInstaller::AddOserver(Observer* observer) {
  observers_.AddObserver(observer);
}

}  // namespace on_device_translation
