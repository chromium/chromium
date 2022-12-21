// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/shortcut_handling_sub_manager.h"

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
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
  DCHECK(!desired_state.has_shortcut());

  if (!registrar_->IsInstalled(app_id)) {
    std::move(configure_done).Run();
    return;
  }

  desired_state.clear_shortcut();

  auto* shortcut = desired_state.mutable_shortcut();
  shortcut->set_title(registrar_->GetAppShortName(app_id));
  shortcut->set_description(registrar_->GetAppDescription(app_id));
  icon_manager_->ReadIconsLastUpdateTime(
      app_id, base::BindOnce(&ShortcutHandlingSubManager::StoreIconDataFromDisk,
                             weak_ptr_factory_.GetWeakPtr(), shortcut)
                  .Then(std::move(configure_done)));
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
    proto::ShortcutDescription* shortcut,
    base::flat_map<SquareSizePx, base::Time> time_map) {
  for (const auto& [size, time] : time_map) {
    auto* shortcut_icon_data = shortcut->add_icon_data_any();
    shortcut_icon_data->set_icon_size(size);
    shortcut_icon_data->set_timestamp(syncer::TimeToProtoTime(time));
  }
}

}  // namespace web_app
