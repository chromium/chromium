// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/garbage_collect_storage_partitions_command.h"

#include <memory>
#include <string>
#include <unordered_set>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"

namespace web_app {

GarbageCollectStoragePartititonsCommand::
    GarbageCollectStoragePartititonsCommand(Profile* profile,
                                            base::OnceClosure done)
    : WebAppCommandTemplate<AllAppsLock>(
          "GarbageCollectStoragePartititonsCommand"),
      lock_description_(std::make_unique<AllAppsLockDescription>()),
      profile_(profile),
      done_closure_(std::move(done)) {
  DCHECK(profile);
}

GarbageCollectStoragePartititonsCommand::
    ~GarbageCollectStoragePartititonsCommand() = default;

void GarbageCollectStoragePartititonsCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  lock_ = std::move(lock);

  ResetStorageGarbageCollectPref();
}

void GarbageCollectStoragePartititonsCommand::ResetStorageGarbageCollectPref() {
  base::OnceClosure callback =
      base::BindOnce(&GarbageCollectStoragePartititonsCommand::OnPrefReset,
                     weak_factory_.GetWeakPtr());

  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, std::move(callback));

  // TODO(crbug.com/1477027): change this pref to be stateful instead of
  // resetting to false early.
  profile_->GetPrefs()->SetBoolean(
      prefs::kShouldGarbageCollectStoragePartitions, false);
  // Waits for both prefs to be written to disk before proceeding to prevent
  // repeating crashes.
  lock_->extensions_manager().ResetStorageGarbageCollectPref(barrier_closure);
  profile_->GetPrefs()->CommitPendingWrite(barrier_closure);
}

void GarbageCollectStoragePartititonsCommand::OnPrefReset() {
  extensions::OnExtensionSystemReady(
      profile_,
      base::BindOnce(
          &GarbageCollectStoragePartititonsCommand::DoGarbageCollection,
          weak_factory_.GetWeakPtr()));
}

const LockDescription&
GarbageCollectStoragePartititonsCommand::lock_description() const {
  return *lock_description_;
}

base::Value GarbageCollectStoragePartititonsCommand::ToDebugValue() const {
  return base::Value(debug_info_.Clone());
}

void GarbageCollectStoragePartititonsCommand::DoGarbageCollection() {
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

  base::Value::List* debug_paths = debug_info_.EnsureList("allow_list_paths");
  for (const auto& path : allowlist) {
    debug_paths->Append(path.LossyDisplayName());
  }

  profile_->GarbageCollectStoragePartitions(
      allowlist,
      base::BindOnce(&GarbageCollectStoragePartititonsCommand::OnSuccess,
                     weak_factory_.GetWeakPtr()));
}

void GarbageCollectStoragePartititonsCommand::OnShutdown() {
  SignalCompletionAndSelfDestruct(CommandResult::kShutdown, base::DoNothing());
}

void GarbageCollectStoragePartititonsCommand::OnSuccess() {
  lock_->extensions_manager()
      .on_garbage_collect_storage_partitions_done_for_testing()  // IN-TEST
      .Signal();

  SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                  std::move(done_closure_));
}

}  // namespace web_app
