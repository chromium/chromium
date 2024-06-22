// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions_manager.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_gate.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace web_app {

// This class registers itself to ExtensionService on construction and
// unregisters itself on destruction. It always delays extension install.
class ExtensionInstallGateImpl : public extensions::InstallGate,
                                 public ExtensionInstallGate {
 public:
  explicit ExtensionInstallGateImpl(Profile* profile)
      : extension_service_(
            extensions::ExtensionSystem::Get(profile)->extension_service()) {
    CHECK(extension_service_);
    extension_service_->RegisterInstallGate(
        extensions::ExtensionPrefs::DelayReason::kGc,
        static_cast<ExtensionInstallGateImpl*>(this));
  }

  ~ExtensionInstallGateImpl() override {
    extension_service_->UnregisterInstallGate(this);
  }

  extensions::InstallGate::Action ShouldDelay(
      const extensions::Extension* extension,
      bool install_immediately) override {
    return extensions::InstallGate::DELAY;
  }

 private:
  raw_ptr<extensions::ExtensionService> extension_service_ = nullptr;
};

ExtensionInstallGate::~ExtensionInstallGate() = default;

ExtensionsManager::ExtensionsManager(Profile* profile)
    : profile_(profile),
      registry_(extensions::ExtensionRegistry::Get(profile)) {}

ExtensionsManager::~ExtensionsManager() = default;

std::unordered_set<base::FilePath>
ExtensionsManager::GetIsolatedStoragePaths() {
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

bool ExtensionsManager::ShouldGarbageCollectStoragePartitions() {
  // `ExtensionPrefs` can be created lazily, so we don't need to wait on
  // extension service.
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile_);
  return extension_prefs && extension_prefs->NeedsStorageGarbageCollection();
}

void ExtensionsManager::ResetStorageGarbageCollectPref(
    base::OnceClosure callback) {
  // `ExtensionPrefs` can be created lazily, so we don't need to wait on
  // extension service.
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile_);
  if (extension_prefs) {
    extension_prefs->pref_service()->SetBoolean(
        extensions::pref_names::kStorageGarbageCollect, false);
    extension_prefs->pref_service()->CommitPendingWrite(std::move(callback));
  }
}

std::unique_ptr<ExtensionInstallGate>
ExtensionsManager::RegisterGarbageCollectionInstallGate() {
  return std::make_unique<web_app::ExtensionInstallGateImpl>(profile_);
}

KeyedServiceBaseFactory* ExtensionsManager::GetExtensionSystemSharedFactory() {
  return extensions::ExtensionSystemSharedFactory::GetInstance();
}

}  // namespace web_app
