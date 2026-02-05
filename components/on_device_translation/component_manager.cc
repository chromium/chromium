// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/component_manager.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/prefs/pref_service.h"

namespace on_device_translation {
namespace {

// The implementation of ComponentManager.
class ComponentManagerImpl : public ComponentManager {
 public:
  ComponentManagerImpl(const ComponentManagerImpl&) = delete;
  ComponentManagerImpl& operator=(const ComponentManagerImpl&) = delete;

  void RegisterTranslateKitComponentImpl() override {
    OnDeviceTranslationInstaller::GetInstance()->Init(base::DoNothing());
  }

  void RegisterTranslateKitLanguagePackComponent(
      LanguagePackKey language_pack) override {
    OnDeviceTranslationInstaller::GetInstance()->InstallLanguagePack(
        language_pack);
  }

  void UninstallTranslateKitLanguagePackComponent(
      LanguagePackKey language_pack) override {
    // Uninstalls the TranslateKit language pack component.
    OnDeviceTranslationInstaller::GetInstance()->UnInstallLanguagePack(
        language_pack);
  }

  base::FilePath GetTranslateKitComponentPathImpl() override {
    // Returns the path from the component updater.
    base::FilePath components_dir;
    base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                           &components_dir);
    CHECK(!components_dir.empty());
    return components_dir.Append(GetBinaryRelativeInstallDir());
  }

 private:
  friend base::NoDestructor<ComponentManagerImpl>;
  ComponentManagerImpl() = default;
  ~ComponentManagerImpl() override = default;
  base::WeakPtrFactory<ComponentManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
ComponentManager* ComponentManager::component_manager_for_test_ = nullptr;

// static
ComponentManager& ComponentManager::GetInstance() {
  // If there is a testing manager, use it.
  if (component_manager_for_test_) {
    return *component_manager_for_test_;
  }
  // Otherwise, use the production manager.
  static base::NoDestructor<ComponentManagerImpl> instance;
  return *instance.get();
}

// static
base::AutoReset<ComponentManager*> ComponentManager::SetForTesting(
    ComponentManager* manager) {
  return base::AutoReset<ComponentManager*>(&component_manager_for_test_,
                                            manager);
}

ComponentManager::ComponentManager() = default;
ComponentManager::~ComponentManager() = default;

bool ComponentManager::RegisterTranslateKitComponent() {
  // Only register the component once.
  if (translate_kit_component_registered_) {
    return false;
  }
  translate_kit_component_registered_ = true;
  RegisterTranslateKitComponentImpl();
  return true;
}

// static
std::set<LanguagePackKey> ComponentManager::GetRegisteredLanguagePacks() {
  return OnDeviceTranslationInstaller::GetInstance()->RegisteredLanguagePacks();
}

// static
std::set<LanguagePackKey> ComponentManager::GetInstalledLanguagePacks() {
  return OnDeviceTranslationInstaller::GetInstance()->InstalledLanguagePacks();
}

// static
bool ComponentManager::HasTranslateKitLibraryPathFromCommandLine() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kTranslateKitBinaryPath);
}

base::FilePath ComponentManager::GetTranslateKitComponentPath() {
  // If the path is specified from the command line, use it.
  auto path_from_command_line = GetTranslateKitBinaryPathFromCommandLine();
  if (!path_from_command_line.empty()) {
    return path_from_command_line;
  }
  return GetTranslateKitComponentPathImpl();
}

}  // namespace on_device_translation
