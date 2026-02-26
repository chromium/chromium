// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/resolve_web_app_pending_migration_info_command.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/types/pass_key.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "chrome/browser/web_applications/model/pending_migration_info.h"
#include "chrome/browser/web_applications/proto/web_app.equal.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app.to_value.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace web_app {

ResolveWebAppPendingMigrationInfoCommand::
    ResolveWebAppPendingMigrationInfoCommand(base::OnceClosure callback)
    : WebAppCommand<AllAppsLock>("ResolveWebAppPendingMigrationInfoCommand",
                                 AllAppsLockDescription(),
                                 std::move(callback)) {}

ResolveWebAppPendingMigrationInfoCommand::
    ~ResolveWebAppPendingMigrationInfoCommand() = default;

void ResolveWebAppPendingMigrationInfoCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  lock_ = std::move(lock);

  base::DictValue& debug_value = GetMutableDebugValue();
  base::DictValue* debug_updates = debug_value.EnsureDict("updates");

  // Map from source_manifest_id (the app being migrated FROM) to list of
  // PendingMigrationInfo (info about the app being migrated TO).
  absl::flat_hash_map<webapps::ManifestId, std::vector<PendingMigrationInfo>>
      pending_migrations;

  for (const WebApp& app : lock_->registrar().GetApps()) {
    // If 'app' claims to be migrated from 'source', then 'source' should know
    // about 'app' via PendingMigrationInfo.
    for (const auto& source : app.validated_migration_sources()) {
      CHECK(source.has_manifest_id());
      CHECK(source.has_behavior());
      CHECK(IsValidProtoMigrationBehavior(source.behavior()));
      PendingMigrationInfo info(app.manifest_id(),
                                FromProtoMigrationBehavior(source.behavior()));
      pending_migrations[webapps::ManifestId(source.manifest_id())].push_back(
          std::move(info));
    }
  }

  std::vector<webapps::AppId> apps_to_notify;
  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    for (const WebApp& app : lock_->registrar().GetAppsIncludingStubs()) {
      webapps::ManifestId manifest_id = app.manifest_id();
      std::optional<PendingMigrationInfo> new_info;
      auto it = pending_migrations.find(manifest_id);
      if (it != pending_migrations.end() && !it->second.empty()) {
        std::vector<PendingMigrationInfo>& infos = it->second;
        // If there are multiple possible migration targets for one app, we just
        // pick an arbitrary one.
        new_info = std::move(infos[0]);
      }

      std::optional<PendingMigrationInfo> current_info =
          app.pending_migration_info();

      if (new_info != current_info) {
        debug_updates->Set(
            app.app_id(),
            base::DictValue()
                .Set("old", current_info ? current_info->AsDebugValue()
                                         : base::Value())
                .Set("new",
                     new_info ? new_info->AsDebugValue() : base::Value()));

        WebApp* mutable_app = update->UpdateApp(app.app_id());
        mutable_app->SetPendingMigrationInfo(std::move(new_info));
        apps_to_notify.push_back(app.app_id());
      }
    }
  }

  for (const auto& app_id : apps_to_notify) {
    const WebApp* app = lock_->registrar().GetAppById(app_id);
    // App could be deleted? Unlikely in this command since we hold the lock,
    // but good to check.
    if (app) {
      lock_->registrar().NotifyWebAppPendingMigrationInfoChanged(
          app_id, app->pending_migration_info().has_value(),
          base::PassKey<ResolveWebAppPendingMigrationInfoCommand>());
    }
  }

  CompleteAndSelfDestruct(CommandResult::kSuccess);
}

}  // namespace web_app
