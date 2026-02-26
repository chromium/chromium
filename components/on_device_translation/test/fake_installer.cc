// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/test/fake_installer.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/threading/thread_restrictions.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"

namespace on_device_translation {

FakeOnDeviceTranslationInstaller::~FakeOnDeviceTranslationInstaller() = default;
FakeOnDeviceTranslationInstaller::FakeOnDeviceTranslationInstaller(
    base::FilePath fake_install_dir)
    : fake_install_dir_(fake_install_dir) {}

bool FakeOnDeviceTranslationInstaller::IsInit() const {
  return is_init_;
}
std::set<LanguagePackKey>
FakeOnDeviceTranslationInstaller::RegisteredLanguagePacks() const {
  return registered_lang_packs_;
}
std::set<LanguagePackKey>
FakeOnDeviceTranslationInstaller::InstalledLanguagePacks() const {
  return installed_lang_packs_;
}
base::FilePath FakeOnDeviceTranslationInstaller::GetLibraryPath() const {
  return fake_install_dir_.AppendASCII("fake_installation.so");
}

base::FilePath FakeOnDeviceTranslationInstaller::GetLanguagePackPath(
    LanguagePackKey language_pack) const {
  return fake_install_dir_.AppendASCII(GetPackageInstallDirName(language_pack));
}

void FakeOnDeviceTranslationInstaller::Init(
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
void FakeOnDeviceTranslationInstaller::InstallLanguagePack(
    LanguagePackKey language_pack) {
  registered_lang_packs_.insert(language_pack);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeOnDeviceTranslationInstaller::InstallLanguagePackNow,
                     weak_ptr_factory_.GetWeakPtr(), language_pack));
}
void FakeOnDeviceTranslationInstaller::InstallLanguagePackNow(
    LanguagePackKey language_pack) {
  registered_lang_packs_.insert(language_pack);
  installed_lang_packs_.insert(language_pack);
  for (Observer& observer : observers_) {
    observer.OnLanguagePackInstalled(language_pack);
  }
}
void FakeOnDeviceTranslationInstaller::UnInstallLanguagePack(
    LanguagePackKey language_pack) {
  registered_lang_packs_.erase(language_pack);
  installed_lang_packs_.erase(language_pack);
}

void FakeOnDeviceTranslationInstaller::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeOnDeviceTranslationInstaller::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace on_device_translation
