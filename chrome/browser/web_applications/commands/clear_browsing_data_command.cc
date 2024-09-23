// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"

#include <vector>

#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/visited_manifest_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

void ClearWebAppBrowsingData(const base::Time& begin_time,
                             const base::Time& end_time,
                             AllAppsLock& lock,
                             base::Value::Dict& debug_value) {
  DCHECK_LE(begin_time, end_time);

  WebAppSyncBridge* sync_bridge = &lock.sync_bridge();
  WebAppRegistrar* registrar = &lock.registrar();
  std::vector<webapps::AppId> ids_to_notify_last_launch_time;
  std::vector<webapps::AppId> ids_to_notify_last_badging_time;
  {
    ScopedRegistryUpdate update = sync_bridge->BeginUpdate();
    for (const WebApp& web_app : registrar->GetApps()) {
      // Only update and notify web apps that have the last launch time set.
      if (!web_app.last_launch_time().is_null() &&
          web_app.last_launch_time() >= begin_time &&
          web_app.last_launch_time() <= end_time) {
        WebApp* mutable_web_app = update->UpdateApp(web_app.app_id());
        if (mutable_web_app) {
          mutable_web_app->SetLastLaunchTime(base::Time());
          ids_to_notify_last_launch_time.push_back(web_app.app_id());
        }
      }
      if (!web_app.last_badging_time().is_null() &&
          web_app.last_badging_time() >= begin_time &&
          web_app.last_badging_time() <= end_time) {
        WebApp* mutable_web_app = update->UpdateApp(web_app.app_id());
        if (mutable_web_app) {
          mutable_web_app->SetLastBadgingTime(base::Time());
          ids_to_notify_last_badging_time.push_back(web_app.app_id());
        }
      }
    }
  }
  base::Value::List* launch_time_removed_debug_list =
      debug_value.EnsureList("last_launch_time_removed");
  for (const webapps::AppId& app_id : ids_to_notify_last_launch_time) {
    launch_time_removed_debug_list->Append(app_id);
    registrar->NotifyWebAppLastLaunchTimeChanged(app_id, base::Time());
  }
  base::Value::List* last_badging_time_removed_debug_list =
      debug_value.EnsureList("last_badging_time_removed");
  for (const webapps::AppId& app_id : ids_to_notify_last_badging_time) {
    last_badging_time_removed_debug_list->Append(app_id);
    registrar->NotifyWebAppLastBadgingTimeChanged(app_id, base::Time());
  }
  lock.visited_manifest_manager().ClearSeenScopes(begin_time, end_time);
}

}  // namespace web_app
