// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_service/exo_app_type_resolver.h"

#include <optional>
#include <string_view>

#include "ash/components/arc/arc_util.h"
#include "ash/wm/window_properties.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chromeos/ash/components/borealis/borealis_util.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/permission.h"
#include "components/exo/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/class_property.h"

namespace {

// Returns true, if the given ID represents Lacros.
bool IsLacrosAppId(std::string_view app_id) {
  return base::StartsWith(app_id, crosapi::kLacrosAppIdPrefix);
}

// Adds ARC specific properties.
void UpdatePropertiesForArc(std::optional<int> task_id,
                            std::optional<int> session_id,
                            exo::ProtectedNativePixmapQueryDelegate*
                                protected_native_pixmap_query_client,
                            ui::PropertyHandler& out_properties_container) {
  out_properties_container.SetProperty(chromeos::kAppTypeKey,
                                       chromeos::AppType::ARC_APP);

  out_properties_container.SetProperty(exo::kProtectedNativePixmapQueryDelegate,
                                       protected_native_pixmap_query_client);

  out_properties_container.SetProperty(
      chromeos::kShouldHaveHighlightBorderOverlay, true);

  if (task_id.has_value())
    out_properties_container.SetProperty(app_restore::kWindowIdKey, *task_id);

  int32_t restore_window_id = 0;
  if (task_id.has_value()) {
    restore_window_id = app_restore::GetArcRestoreWindowIdForTaskId(*task_id);
  } else {
    DCHECK(session_id.has_value());
    out_properties_container.SetProperty(app_restore::kGhostWindowSessionIdKey,
                                         *session_id);
    restore_window_id =
        app_restore::GetArcRestoreWindowIdForSessionId(*session_id);
  }

  out_properties_container.SetProperty(app_restore::kRestoreWindowIdKey,
                                       restore_window_id);

  if (restore_window_id == app_restore::kParentToHiddenContainer) {
    out_properties_container.SetProperty(
        app_restore::kParentToHiddenContainerKey, true);
  }

  out_properties_container.SetProperty(aura::client::kSkipImeProcessing, true);
}

}  // namespace

void ExoAppTypeResolver::PopulateProperties(
    const Params& params,
    ui::PropertyHandler& out_properties_container) {
  if (IsLacrosAppId(params.app_id)) {
    out_properties_container.SetProperty(chromeos::kAppTypeKey,
                                         chromeos::AppType::LACROS);
    // Lacros is trusted not to abuse window activation, so grant it a
    // non-expiring permission to activate.
    out_properties_container.SetProperty(
        exo::kPermissionKey,
        new exo::Permission(exo::Permission::Capability::kActivate));
    // Only Lacros windows should allow restore/fullscreen to kick windows out
    // of fullscreen.
    out_properties_container.SetProperty(exo::kRestoreOrMaximizeExitsFullscreen,
                                         true);
    out_properties_container.SetProperty(app_restore::kLacrosWindowId,
                                         params.app_id);

    out_properties_container.SetProperty(ash::kWebAuthnRequestId,
                                         new std::string(params.app_id));
    return;
  }

  auto task_id = arc::GetTaskIdFromWindowAppId(params.app_id);
  auto session_id = arc::GetSessionIdFromWindowAppId(params.app_id);

  // If |task_id| or |session_id| are valid, this is an ARC window.
  if (task_id.has_value() || session_id.has_value()) {
    UpdatePropertiesForArc(
        task_id, session_id,
        reinterpret_cast<exo::ProtectedNativePixmapQueryDelegate*>(
            &protected_native_pixmap_query_client_),
        out_properties_container);
  }

  // GuestOS VMs
  if (plugin_vm::IsPluginvmWindowId(params.app_id)) {
    return;
  }

  out_properties_container.SetProperty(exo::kMaximumSizeForResizabilityOnly,
                                       true);
  if (ash::borealis::IsBorealisWindowId(
          params.app_id.empty() ? params.startup_id : params.app_id)) {
    // TODO(b/165865831): Stop using CROSTINI_APP for borealis windows.
    out_properties_container.SetProperty(chromeos::kAppTypeKey,
                                         chromeos::AppType::CROSTINI_APP);

    // Auto-maximize causes compatibility issues, and we don't need it anyway.
    out_properties_container.SetProperty(chromeos::kAutoMaximizeXdgShellEnabled,
                                         false);
    return;
  }
}
