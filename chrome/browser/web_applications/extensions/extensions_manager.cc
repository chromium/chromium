// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace web_app {

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

}  // namespace web_app
