// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/extensions_manager_impl.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/delayed_install_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install_gate.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace web_app {

// This class registers itself to DelayedInstallManager on construction and
// unregisters itself on destruction. It always delays extension install.
class ExtensionInstallGateImpl : public extensions::InstallGate,
                                 public ExtensionInstallGate {
 public:
  explicit ExtensionInstallGateImpl(Profile* profile) : profile_(profile) {
    CHECK(profile);
    extensions::DelayedInstallManager::Get(profile)->RegisterInstallGate(
        extensions::ExtensionPrefs::DelayReason::kGc,
        static_cast<ExtensionInstallGateImpl*>(this));
  }

  ~ExtensionInstallGateImpl() override {
    extensions::DelayedInstallManager::Get(profile_)->UnregisterInstallGate(
        this);
  }

  extensions::InstallGate::Action ShouldDelay(
      const extensions::Extension* extension,
      bool install_immediately) override {
    return extensions::InstallGate::DELAY;
  }

 private:
  raw_ptr<Profile> profile_ = nullptr;
};

ExtensionsManagerImpl::ExtensionsManagerImpl(Profile* profile)
    : profile_(profile),
      registry_(extensions::ExtensionRegistry::Get(profile)) {}

ExtensionsManagerImpl::~ExtensionsManagerImpl() = default;

void ExtensionsManagerImpl::OnExtensionSystemReady(base::OnceClosure closure) {
  extensions::ExtensionSystem::Get(profile_)->ready().Post(FROM_HERE,
                                                           std::move(closure));
}

std::unordered_set<base::FilePath>
ExtensionsManagerImpl::GetIsolatedStoragePaths() {
  std::unordered_set<base::FilePath> allowlist;
  extensions::ExtensionSet extensions =
      registry_->GenerateInstalledExtensionsSet();
  for (const auto& ext : extensions) {
    if (extensions::util::HasIsolatedStorage(*ext.get(), profile_)) {
      allowlist.insert(extensions::util::GetStoragePartitionForExtensionId(
                           ext->id(), profile_)
                           ->GetPath());
    }
  }
  return allowlist;
}

std::unique_ptr<ExtensionInstallGate>
ExtensionsManagerImpl::RegisterGarbageCollectionInstallGate() {
  return std::make_unique<web_app::ExtensionInstallGateImpl>(profile_);
}

// Implementation of ExtensionsManager static methods:
// static
std::unique_ptr<ExtensionsManager> ExtensionsManager::CreateForProfile(
    Profile* profile) {
  return std::make_unique<ExtensionsManagerImpl>(profile);
}
// static
KeyedServiceBaseFactory* ExtensionsManager::GetExtensionSystemSharedFactory() {
  return extensions::ChromeExtensionSystemSharedFactory::GetInstance();
}

}  // namespace web_app
