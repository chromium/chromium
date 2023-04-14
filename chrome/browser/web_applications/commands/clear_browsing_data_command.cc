// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/clear_browsing_data_command.h"

#include <vector>

#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

void ClearWebAppBrowsingData(const base::Time& begin_time,
                             const base::Time& end_time,
                             base::OnceClosure done,
                             AllAppsLock& lock) {
  DCHECK_LE(begin_time, end_time);

  WebAppSyncBridge* sync_bridge = &lock.sync_bridge();
  WebAppRegistrar* registrar = &lock.registrar();
  std::vector<AppId> ids_to_notify_last_launch_time;
  std::vector<AppId> ids_to_notify_last_badging_time;
  {
    ScopedRegistryUpdate update(sync_bridge);
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
  for (const AppId& app_id : ids_to_notify_last_launch_time) {
    registrar->NotifyWebAppLastLaunchTimeChanged(app_id, base::Time());
  }
  for (const AppId& app_id : ids_to_notify_last_badging_time) {
    registrar->NotifyWebAppLastBadgingTimeChanged(app_id, base::Time());
  }

  std::move(done).Run();
}

}  // namespace web_app
