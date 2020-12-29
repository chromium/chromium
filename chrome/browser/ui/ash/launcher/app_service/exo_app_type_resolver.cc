// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service/exo_app_type_resolver.h"

#include "ash/public/cpp/app_types.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/chromeos/borealis/borealis_window_manager.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "components/arc/arc_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/class_property.h"

namespace {

// Returns true, if the given ID represents Lacros.
bool IsLacrosAppId(base::StringPiece app_id) {
  return base::StartsWith(app_id, crosapi::kLacrosAppIdPrefix);
}

}  // namespace

void ExoAppTypeResolver::PopulateProperties(
    const std::string& app_id,
    const std::string& startup_id,
    bool for_creation,
    ui::PropertyHandler& out_properties_container) {
  if (IsLacrosAppId(app_id)) {
    out_properties_container.SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::LACROS));
  } else if (arc::GetTaskIdFromWindowAppId(app_id) != arc::kNoTaskId) {
    out_properties_container.SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::ARC_APP));
  } else if (borealis::BorealisWindowManager::IsBorealisWindowId(
                 app_id.empty() ? startup_id : app_id)) {
    // TODO(b/165865831): Stop using CROSTINI_APP for borealis windows.
    out_properties_container.SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::CROSTINI_APP));
  }
}
