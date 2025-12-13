// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/update_ignore_state.h"

#include "base/values.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app.to_value.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

void SetWebAppPendingUpdateAsIgnored(const webapps::AppId& app_id,
                                     AppLock& lock,
                                     base::Value::Dict& debug_value) {
  debug_value.Set("app_id", app_id);

  // Exit early if the app is not installed or the app does not have a pending
  // update info.
  if (!lock.registrar().AppMatches(app_id, WebAppFilter::InstalledInChrome())) {
    debug_value.Set("result", "App not installed.");
    return;
  }

  const WebApp* web_app = lock.registrar().GetAppById(app_id);
  if (!web_app->pending_update_info().has_value() ||
      !web_app->pending_update_info()->has_was_ignored()) {
    debug_value.Set("result", "No pending info in app");
    return;
  }

  // If the app was already ignored, no need of doing a lot of work and updating
  // the app again.
  if (web_app->pending_update_info()->was_ignored()) {
    debug_value.Set("result", "Pending update already ignored");
    return;
  }

  // Set the information in the app that the pending update info was ignored.
  {
    ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
    WebApp* app_to_update = update->UpdateApp(app_id);
    proto::PendingUpdateInfo update_info =
        *app_to_update->pending_update_info();
    update_info.set_was_ignored(true);
    debug_value.Set("result", "Pending Info updated");
    debug_value.Set("updated_pending_info", proto::ToValue(update_info));
    app_to_update->SetPendingUpdateInfo(std::move(update_info));
  }

  // Since the pending update is being set as ignored, the menu button should
  // not be shown again.
  lock.registrar().NotifyPendingUpdateInfoChanged(
      app_id, /*pending_update_available=*/false,
      WebAppRegistrar::PendingUpdateInfoChangePassKey());
}

}  // namespace web_app
