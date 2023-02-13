// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/shortcut_menu_handling_sub_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
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

  proto::ShortcutMenus* shortcut_menus = desired_state.mutable_shortcut_menus();
  icon_manager_->ReadAllShortcutMenuIconsWithTimestamp(
      app_id,
      base::BindOnce(&ShortcutMenuHandlingSubManager::StoreShortcutMenuData,
                     weak_ptr_factory_.GetWeakPtr(), app_id, shortcut_menus)
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
    const AppId& app_id,
    proto::ShortcutMenus* shortcut_menus,
    WebAppIconManager::ShortcutIconDataVector shortcut_menu_items) {
  if (shortcut_menu_items.size() == 0) {
    return;
  }
  std::vector<WebAppShortcutsMenuItemInfo> shortcut_menu_item_info =
      registrar_->GetAppShortcutsMenuItemInfos(app_id);
  DCHECK_EQ(shortcut_menu_item_info.size(), shortcut_menu_items.size());
  for (size_t menu_index = 0; menu_index < shortcut_menu_items.size();
       menu_index++) {
    proto::ShortcutMenuInfo* new_shortcut_menu_item =
        shortcut_menus->add_shortcut_menu_info();
    new_shortcut_menu_item->set_shortcut_name(
        base::UTF16ToUTF8(shortcut_menu_item_info[menu_index].name));
    new_shortcut_menu_item->set_shortcut_launch_url(
        shortcut_menu_item_info[menu_index].url.spec());

    for (const auto& [size, time] :
         shortcut_menu_items[menu_index][IconPurpose::ANY]) {
      web_app::proto::ShortcutIconData* icon_data =
          new_shortcut_menu_item->add_icon_data_any();
      icon_data->set_icon_size(size);
      icon_data->set_timestamp(syncer::TimeToProtoTime(time));
    }

    for (const auto& [size, time] :
         shortcut_menu_items[menu_index][IconPurpose::MASKABLE]) {
      web_app::proto::ShortcutIconData* icon_data =
          new_shortcut_menu_item->add_icon_data_maskable();
      icon_data->set_icon_size(size);
      icon_data->set_timestamp(syncer::TimeToProtoTime(time));
    }

    for (const auto& [size, time] :
         shortcut_menu_items[menu_index][IconPurpose::MONOCHROME]) {
      web_app::proto::ShortcutIconData* icon_data =
          new_shortcut_menu_item->add_icon_data_monochrome();
      icon_data->set_icon_size(size);
      icon_data->set_timestamp(syncer::TimeToProtoTime(time));
    }
  }
}

}  // namespace web_app
