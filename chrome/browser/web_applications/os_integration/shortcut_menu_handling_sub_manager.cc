// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/shortcut_menu_handling_sub_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/base/time.h"

namespace web_app {
ShortcutMenuHandlingSubManager::ShortcutMenuHandlingSubManager(
    WebAppIconManager& icon_manager,
    WebAppRegistrar& registrar)
    : icon_manager_(icon_manager), registrar_(registrar) {}

ShortcutMenuHandlingSubManager::~ShortcutMenuHandlingSubManager() = default;

void ShortcutMenuHandlingSubManager::Start() {}

void ShortcutMenuHandlingSubManager::Shutdown() {}

void ShortcutMenuHandlingSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_shortcut_menus());

  if (!registrar_->IsLocallyInstalled(app_id)) {
    std::move(configure_done).Run();
    return;
  }

  std::string url = registrar_->GetAppLaunchUrl(app_id).spec();
  std::string title = registrar_->GetAppShortName(app_id);
  proto::ShortcutMenus* shortcut_menus = desired_state.mutable_shortcut_menus();
  icon_manager_->ReadAllShortcutMenuIconsWithTimestamp(
      app_id,
      base::BindOnce(&ShortcutMenuHandlingSubManager::StoreShortcutMenuData,
                     weak_ptr_factory_.GetWeakPtr(), shortcut_menus, title, url)
          .Then(std::move(configure_done)));
}

void ShortcutMenuHandlingSubManager::Execute(
    const AppId& app_id,
    const absl::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure callback) {
  // Not implemented yet.
  std::move(callback).Run();
}

void ShortcutMenuHandlingSubManager::StoreShortcutMenuData(
    proto::ShortcutMenus* shortcut_menus,
    std::string title,
    std::string url,
    WebAppIconManager::ShortcutIconDataVector shortcut_menu_items) {
  for (auto& menu_item : shortcut_menu_items) {
    proto::ShortcutMenuInfo* new_shortcut_menu_item =
        shortcut_menus->add_shortcut_menu_info();
    new_shortcut_menu_item->set_app_title(title);
    new_shortcut_menu_item->set_app_launch_url(url);

    for (const auto& [size, time] : menu_item[IconPurpose::ANY]) {
      web_app::proto::ShortcutIconData* icon_data =
          new_shortcut_menu_item->add_icon_data_any();
      icon_data->set_icon_size(size);
      icon_data->set_timestamp(syncer::TimeToProtoTime(time));
    }

    for (const auto& [size, time] : menu_item[IconPurpose::MASKABLE]) {
      web_app::proto::ShortcutIconData* icon_data =
          new_shortcut_menu_item->add_icon_data_maskable();
      icon_data->set_icon_size(size);
      icon_data->set_timestamp(syncer::TimeToProtoTime(time));
    }

    for (const auto& [size, time] : menu_item[IconPurpose::MONOCHROME]) {
      web_app::proto::ShortcutIconData* icon_data =
          new_shortcut_menu_item->add_icon_data_monochrome();
      icon_data->set_icon_size(size);
      icon_data->set_timestamp(syncer::TimeToProtoTime(time));
    }
  }
}

}  // namespace web_app
