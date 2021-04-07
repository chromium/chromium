// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/exo_app_type_resolver.h"

#include "ash/public/cpp/app_types.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/arc/arc_util.h"
#include "components/exo/permission.h"
#include "components/full_restore/full_restore_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/class_property.h"

namespace {

// Returns true, if the given ID represents Lacros.
bool IsLacrosAppId(base::StringPiece app_id) {
  return base::StartsWith(app_id, crosapi::kLacrosAppIdPrefix);
}

}  // namespace

void ExoAppTypeResolver::PopulateProperties(
    const Params& params,
    ui::PropertyHandler& out_properties_container) {
  if (IsLacrosAppId(params.app_id)) {
    out_properties_container.SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::LACROS));
    // Lacros is trusted not to abuse window activation, so grant it a
    // non-expiring permission to activate.
    out_properties_container.SetProperty(
        exo::kPermissionKey,
        new exo::Permission(exo::Permission::Capability::kActivate));
  } else if (borealis::BorealisWindowManager::IsBorealisWindowId(
                 params.app_id.empty() ? params.startup_id : params.app_id)) {
    // TODO(b/165865831): Stop using CROSTINI_APP for borealis windows.
    out_properties_container.SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::CROSTINI_APP));

    // Auto-maximize causes compatibility issues, and we don't need it anyway.
    out_properties_container.SetProperty(chromeos::kAutoMaximizeXdgShellEnabled,
                                         false);
  }

  int task_id = arc::GetTaskIdFromWindowAppId(params.app_id);
  if (task_id == arc::kNoTaskId)
    return;

  out_properties_container.SetProperty(aura::client::kAppType,
                                       static_cast<int>(ash::AppType::ARC_APP));
  out_properties_container.SetProperty(full_restore::kWindowIdKey, task_id);
  int32_t restore_window_id = full_restore::GetArcRestoreWindowId(task_id);
  out_properties_container.SetProperty(full_restore::kRestoreWindowIdKey,
                                       restore_window_id);

  if (restore_window_id == full_restore::kParentToHiddenContainer) {
    out_properties_container.SetProperty(
        full_restore::kParentToHiddenContainerKey, true);
  }
}
