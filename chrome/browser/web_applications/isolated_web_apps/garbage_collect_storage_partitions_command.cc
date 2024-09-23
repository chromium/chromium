// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/garbage_collect_storage_partitions_command.h"

#include <memory>
#include <string>
#include <unordered_set>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_closures.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"

namespace web_app {

GarbageCollectStoragePartitionsCommand::GarbageCollectStoragePartitionsCommand(
    Profile* profile,
    base::OnceClosure done)
    : WebAppCommand<AllAppsLock>("GarbageCollectStoragePartitionsCommand",
                                 AllAppsLockDescription(),
                                 std::move(done)),
      profile_(profile) {
  DCHECK(profile);
}

GarbageCollectStoragePartitionsCommand::
    ~GarbageCollectStoragePartitionsCommand() = default;

void GarbageCollectStoragePartitionsCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  lock_ = std::move(lock);
  ResetStorageGarbageCollectPref();
}

void GarbageCollectStoragePartitionsCommand::ResetStorageGarbageCollectPref() {
  base::ConcurrentClosures concurrent;
  // TODO(crbug.com/40280176): change this pref to be stateful instead of
  // resetting to false early.
  profile_->GetPrefs()->SetBoolean(
      prefs::kShouldGarbageCollectStoragePartitions, false);
  // Waits for both prefs to be written to disk before proceeding to prevent
  // repeating crashes.
  lock_->extensions_manager().ResetStorageGarbageCollectPref(
      concurrent.CreateClosure());
  profile_->GetPrefs()->CommitPendingWrite(concurrent.CreateClosure());
  std::move(concurrent)
      .Done(base::BindOnce(&GarbageCollectStoragePartitionsCommand::OnPrefReset,
                           weak_factory_.GetWeakPtr()));
}

void GarbageCollectStoragePartitionsCommand::OnPrefReset() {
  extensions::OnExtensionSystemReady(
      profile_,
      base::BindOnce(
          &GarbageCollectStoragePartitionsCommand::DoGarbageCollection,
          weak_factory_.GetWeakPtr()));
}

void GarbageCollectStoragePartitionsCommand::DoGarbageCollection() {
  std::unordered_set<base::FilePath> allowlist;

  // InstallGate delays extension installations.
  install_gate_ =
      lock_->extensions_manager().RegisterGarbageCollectionInstallGate();

  // Get all paths from Extension system.
  {
    ExtensionsManager& extensions_manager = lock_->extensions_manager();
    allowlist.merge(extensions_manager.GetIsolatedStoragePaths());
  }

  // Get all paths from Web App system.
  {
    WebAppRegistrar::AppSet app_set = lock_->registrar().GetApps();
    for (const auto& app : app_set) {
      if (!app.isolation_data().has_value()) {
        continue;
      }
      auto url_info = IsolatedWebAppUrlInfo::Create(app.scope());
      if (url_info.has_value()) {
        allowlist.insert(profile_
                             ->GetStoragePartition(
                                 url_info->storage_partition_config(profile_))
                             ->GetPath());
      }
    }
  }

  base::Value::List* debug_paths =
      GetMutableDebugValue().EnsureList("allow_list_paths");
  for (const auto& path : allowlist) {
    debug_paths->Append(path.LossyDisplayName());
  }

  profile_->GarbageCollectStoragePartitions(
      allowlist,
      base::BindOnce(&GarbageCollectStoragePartitionsCommand::OnSuccess,
                     weak_factory_.GetWeakPtr()));
}

void GarbageCollectStoragePartitionsCommand::OnSuccess() {
  CompleteAndSelfDestruct(CommandResult::kSuccess);
}

}  // namespace web_app
