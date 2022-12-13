// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/shortcut_handling_sub_manager.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/base/time.h"

namespace web_app {

ShortcutHandlingSubManager::ShortcutHandlingSubManager(
    WebAppIconManager& icon_manager,
    WebAppRegistrar& registrar)
    : icon_manager_(icon_manager), registrar_(registrar) {}

ShortcutHandlingSubManager::~ShortcutHandlingSubManager() = default;

void ShortcutHandlingSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_shortcut_states());

  if (!registrar_->IsInstalled(app_id)) {
    std::move(configure_done).Run();
    return;
  }

  desired_state.clear_shortcut_states();

  auto* shortcut_states = desired_state.mutable_shortcut_states();
  shortcut_states->set_title(registrar_->GetAppShortName(app_id));
  shortcut_states->set_description(registrar_->GetAppDescription(app_id));
  icon_manager_->ReadIconsLastUpdateTime(
      app_id, base::BindOnce(&ShortcutHandlingSubManager::StoreIconDataFromDisk,
                             weak_ptr_factory_.GetWeakPtr(), shortcut_states,
                             std::move(configure_done)));
}

void ShortcutHandlingSubManager::Start() {}

void ShortcutHandlingSubManager::Shutdown() {}

void ShortcutHandlingSubManager::Execute(
    const AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    const absl::optional<proto::WebAppOsIntegrationState>& current_state,
    base::OnceClosure callback) {
  NOTREACHED() << "Not yet implemented";
}

void ShortcutHandlingSubManager::StoreIconDataFromDisk(
    proto::ShortcutState* shortcut_states,
    base::OnceClosure configure_done,
    base::flat_map<SquareSizePx, base::Time> time_map) {
  for (const auto& time_data : time_map) {
    auto* shortcut_icon_data = shortcut_states->add_icon_data_any();
    shortcut_icon_data->set_icon_size(time_data.first);
    shortcut_icon_data->set_timestamp(
        syncer::TimeToProtoTime(time_data.second));
  }
  std::move(configure_done).Run();
}

}  // namespace web_app
