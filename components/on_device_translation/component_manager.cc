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

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // BUILDFLAG(IS_WIN)

namespace on_device_translation {
namespace {

const char kTranslateKitPackagePaths[] = "translate-kit-packages";

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
    if (!OnDeviceTranslationInstaller::GetInstance()->IsInit()) {
      OnDeviceTranslationInstaller::GetInstance()->Init(base::BindRepeating(
          &ComponentManagerImpl::RegisterTranslateKitLanguagePackComponent,
          weak_ptr_factory_.GetWeakPtr(), language_pack));
      return;
    }
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

ComponentManager::ComponentManager()
    : language_packs_from_command_line_(GetLanguagePackInfoFromCommandLine()) {}

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

void ComponentManager::GetLanguagePackInfo(
    std::vector<mojom::OnDeviceTranslationLanguagePackagePtr>& packages,
    std::vector<base::FilePath>& package_pathes) {
  CHECK(packages.empty());
  CHECK(package_pathes.empty());
  if (language_packs_from_command_line_) {
    for (const auto& package : *language_packs_from_command_line_) {
      packages.push_back(mojom::OnDeviceTranslationLanguagePackage::New(
          package.language1, package.language2));
      package_pathes.push_back(package.package_path);
    }
    return;
  }

  for (const auto& it : kLanguagePackComponentConfigMap) {
    auto file_path =
        OnDeviceTranslationInstaller::GetInstance()->GetLanguagePackPath(
            it.first);
    if (!file_path.empty()) {
      packages.push_back(mojom::OnDeviceTranslationLanguagePackage::New(
          std::string(ToLanguageCode(it.second->language1)),
          std::string(ToLanguageCode(it.second->language2))));
      package_pathes.push_back(file_path);
    }
  }
}

// static
std::optional<std::vector<ComponentManager::LanguagePackInfo>>
ComponentManager::GetLanguagePackInfoFromCommandLine() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kTranslateKitPackagePaths)) {
    return std::nullopt;
  }
  const auto packages_string =
      command_line->GetSwitchValueNative(kTranslateKitPackagePaths);
  std::vector<base::CommandLine::StringType> splitted_strings =
      base::SplitString(packages_string,
#if BUILDFLAG(IS_WIN)
                        L",",
#else   // !BUILDFLAG(IS_WIN)
                        ",",
#endif  // BUILDFLAG(IS_WIN)
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (splitted_strings.size() % 3 != 0) {
    LOG(ERROR) << "Invalid --" << kTranslateKitPackagePaths << " flag.";
    return std::nullopt;
  }

  std::vector<LanguagePackInfo> packages;
  auto it = splitted_strings.begin();
  while (it != splitted_strings.end()) {
    if (!base::IsStringASCII(*it) || !base::IsStringASCII(*(it + 1))) {
      LOG(ERROR) << "Invalid --" << kTranslateKitPackagePaths << " flag.";
      return std::nullopt;
    }
    LanguagePackInfo package;
#if BUILDFLAG(IS_WIN)
    package.language1 = base::WideToUTF8(*(it++));
    package.language2 = base::WideToUTF8(*(it++));
#else  // !BUILDFLAG(IS_WIN)
    package.language1 = *(it++);
    package.language2 = *(it++);
#endif
    package.package_path = base::FilePath(*(it++));
    packages.push_back(std::move(package));
  }
  return packages;
}

}  // namespace on_device_translation
