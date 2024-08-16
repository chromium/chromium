// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/link_capturing.h"

#include "base/values.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

void SetAppCapturesSupportedLinksDisableOverlapping(
    const webapps::AppId& app_id,
    bool set_to_preferred,
    AllAppsLock& lock,
    base::Value::Dict& debug_value) {
  debug_value.Set("app_id", app_id);
  debug_value.Set("set_to_preferred", set_to_preferred);

  if (!lock.registrar().IsInstallState(
          app_id, {proto::INSTALLED_WITHOUT_OS_INTEGRATION,
                   proto::INSTALLED_WITH_OS_INTEGRATION})) {
    debug_value.Set("result", "App not installed.");
    return;
  }
  if (lock.registrar().IsShortcutApp(app_id)) {
    debug_value.Set("result", "App is created as a shortcut.");
    return;
  }

  // When disabling, simply disable & exit because this doesn't need to affect
  // the state of other apps.
  if (!set_to_preferred) {
    {
      ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
      WebApp* app_to_update = update->UpdateApp(app_id);
      app_to_update->SetLinkCapturingUserPreference(
          proto::LinkCapturingUserPreference::DO_NOT_CAPTURE_SUPPORTED_LINKS);
    }
    debug_value.Set("app_updated", true);
    // TODO(b/273830801): Automatically call observers when changes are
    // committed to the web_app DB (here and below).
    lock.registrar().NotifyWebAppUserLinkCapturingPreferencesChanged(
        app_id, set_to_preferred);
    return;
  }

  // Whe enabling, any app with the same scope (overlapping) that is explicitly
  // enabled must be disabled to prevent multiple apps from capturing links on
  // the same scope.
  for (const webapps::AppId& other_app_id : lock.registrar().GetAppIds()) {
    if (other_app_id == app_id) {
      {
        ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
        WebApp* app_to_update = update->UpdateApp(app_id);
        app_to_update->SetLinkCapturingUserPreference(
            set_to_preferred
                ? proto::LinkCapturingUserPreference::CAPTURE_SUPPORTED_LINKS
                : proto::LinkCapturingUserPreference::
                      DO_NOT_CAPTURE_SUPPORTED_LINKS);
      }
      debug_value.Set("app_updated", true);
      lock.registrar().NotifyWebAppUserLinkCapturingPreferencesChanged(
          app_id, set_to_preferred);
      continue;
    }
    if (!lock.registrar().CanCaptureLinksInScope(other_app_id)) {
      continue;
    }
    if (!lock.registrar().AppScopesMatchForUserLinkCapturing(app_id,
                                                             other_app_id)) {
      continue;
    }
    const WebApp* other_app = lock.registrar().GetAppById(other_app_id);
    CHECK(other_app);
    // Only update apps that are explicitly turned on. We want to leave any set
    // as 'default' to stay as such, allowing them to take over if this one is
    // uninstalled.
    if (other_app->user_link_capturing_preference() !=
        proto::LinkCapturingUserPreference::CAPTURE_SUPPORTED_LINKS) {
      continue;
    }
    {
      ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
      WebApp* app_to_update = update->UpdateApp(other_app_id);
      app_to_update->SetLinkCapturingUserPreference(
          proto::LinkCapturingUserPreference::DO_NOT_CAPTURE_SUPPORTED_LINKS);
    }
    debug_value.EnsureList("capturing_apps_disabled")->Append(other_app_id);
    lock.registrar().NotifyWebAppUserLinkCapturingPreferencesChanged(
        other_app_id, /*is_preferred=*/false);
  }
  return;
}

}  // namespace web_app
