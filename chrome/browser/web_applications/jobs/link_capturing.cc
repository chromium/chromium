// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/link_capturing.h"

#include "base/values.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

base::Value SetAppCapturesSupportedLinksDisableOverlapping(
    const webapps::AppId& app_id,
    bool set_to_preferred,
    AllAppsLock& lock) {
  base::Value::Dict debug_value;
  debug_value.Set("app_id", app_id);
  debug_value.Set("set_to_preferred", set_to_preferred);

  bool is_already_preferred = lock.registrar().CapturesLinksInScope(app_id);

  // Only update in web_app DB if the user selected choice does not match the
  // one in the DB currently.
  bool requires_update = (set_to_preferred && !is_already_preferred) ||
                         (!set_to_preferred && is_already_preferred);

  debug_value.Set("requires_update", requires_update);
  if (!requires_update) {
    return base::Value(std::move(debug_value));
  }

  // TODO(b/273830801): Automatically call observers when changes are committed
  // to the web_app DB.
  for (const webapps::AppId& id : lock.registrar().GetAppIds()) {
    if (id == app_id) {
      {
        ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
        WebApp* app_to_update = update->UpdateApp(app_id);
        app_to_update->SetIsUserSelectedAppForSupportedLinks(set_to_preferred);
      }
      debug_value.Set("app_updated", true);
      lock.registrar().NotifyWebAppUserLinkCapturingPreferencesChanged(
          app_id, set_to_preferred);
    } else {
      // For all other app_ids, if one is already set as the preferred, reset
      // all other apps in the registry if they were previously set to be a
      // preferred app to capture similar type of links according to scope
      // prefixes.
      if (set_to_preferred && lock.registrar().CapturesLinksInScope(id) &&
          lock.registrar().AppScopesMatchForUserLinkCapturing(app_id, id)) {
        {
          ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
          WebApp* app_to_update = update->UpdateApp(id);
          app_to_update->SetIsUserSelectedAppForSupportedLinks(
              /*is_user_selected_app_for_capturing_links=*/false);
        }
        debug_value.EnsureList("capturing_apps_disabled")->Append(id);
        lock.registrar().NotifyWebAppUserLinkCapturingPreferencesChanged(
            id, /*is_preferred=*/false);
      }
    }
  }
  return base::Value(std::move(debug_value));
}

}  // namespace web_app
