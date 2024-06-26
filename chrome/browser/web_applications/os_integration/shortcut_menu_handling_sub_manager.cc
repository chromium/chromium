// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/shortcut_menu_handling_sub_manager.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/sync/base/time.h"
#include "url/gurl.h"

namespace web_app {

namespace {

bool HasShortcutsMenuInfo(const proto::WebAppOsIntegrationState& state) {
  return state.has_shortcut_menus() &&
         state.shortcut_menus().shortcut_menu_info_size() > 0;
}

enum class ShortcutMenuIconDataDiffResult {
  kSuccess = 0,
  kIconCountLessThanItemCount = 1,
  kItemCountLessThanIconCount = 2,
  kMaxValue = kItemCountLessThanIconCount,
};

void MeasureShortcutMenuIconHistograms(int icon_count, int item_count) {
  if (icon_count == item_count) {
    base::UmaHistogramEnumeration(
        "WebApp.ShortcutsMenuBitmapAndItemCount.Results",
        ShortcutMenuIconDataDiffResult::kSuccess);
  } else if (icon_count < item_count) {
    base::UmaHistogramEnumeration(
        "WebApp.ShortcutsMenuBitmapAndItemCount.Results",
        ShortcutMenuIconDataDiffResult::kIconCountLessThanItemCount);
  } else {
    base::UmaHistogramEnumeration(
        "WebApp.ShortcutsMenuBitmapAndItemCount.Results",
        ShortcutMenuIconDataDiffResult::kItemCountLessThanIconCount);
  }
}

}  // namespace

ShortcutMenuHandlingSubManager::ShortcutMenuHandlingSubManager(
    const base::FilePath& profile_path,
    WebAppProvider& provider)
    : profile_path_(profile_path), provider_(provider) {}

ShortcutMenuHandlingSubManager::~ShortcutMenuHandlingSubManager() = default;

void ShortcutMenuHandlingSubManager::Configure(
    const webapps::AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_shortcut_menus());

  if (provider_->registrar_unsafe().GetInstallState(app_id) !=
      proto::INSTALLED_WITH_OS_INTEGRATION) {
    std::move(configure_done).Run();
    return;
  }

  std::vector<WebAppShortcutsMenuItemInfo> shortcut_menu_item_info =
      provider_->registrar_unsafe().GetAppShortcutsMenuItemInfos(app_id);
  if (shortcut_menu_item_info.empty()) {
    std::move(configure_done).Run();
    return;
  }

  proto::ShortcutMenus* shortcut_menus = desired_state.mutable_shortcut_menus();
  provider_->icon_manager().ReadAllShortcutMenuIconsWithTimestamp(
      app_id,
      base::BindOnce(&ShortcutMenuHandlingSubManager::StoreShortcutMenuData,
                     weak_ptr_factory_.GetWeakPtr(), app_id,
                     shortcut_menu_item_info, shortcut_menus)
          .Then(std::move(configure_done)));
}

void ShortcutMenuHandlingSubManager::Execute(
    const webapps::AppId& app_id,
    const std::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure execute_complete) {
  if (!ShouldRegisterShortcutsMenuWithOs()) {
    std::move(execute_complete).Run();
    return;
  }

  // If none of the current and desired states have shortcuts, then this should
  // just be a no-op.
  if (!HasShortcutsMenuInfo(desired_state) &&
      !HasShortcutsMenuInfo(current_state)) {
    std::move(execute_complete).Run();
    return;
  }

  if (HasShortcutsMenuInfo(desired_state) &&
      HasShortcutsMenuInfo(current_state) &&
      (desired_state.shortcut_menus().SerializeAsString() ==
       current_state.shortcut_menus().SerializeAsString())) {
    std::move(execute_complete).Run();
    return;
  }

  CHECK_OS_INTEGRATION_ALLOWED();

  StartShortcutsMenuUnregistration(
      app_id, current_state,
      base::BindOnce(
          &ShortcutMenuHandlingSubManager::ReadIconDataForShortcutsMenu,
          weak_ptr_factory_.GetWeakPtr(), app_id, desired_state,
          std::move(execute_complete)));
}

void ShortcutMenuHandlingSubManager::ForceUnregister(
    const webapps::AppId& app_id,
    base::OnceClosure callback) {
  if (!ShouldRegisterShortcutsMenuWithOs()) {
    std::move(callback).Run();
    return;
  }

  web_app::UnregisterShortcutsMenuWithOs(
      app_id, profile_path_, base::BindOnce([](Result result) {
                               base::UmaHistogramBoolean(
                                   "WebApp.ShortcutsMenuUnregistered.Result",
                                   (result == Result::kOk));
                             }).Then(std::move(callback)));
}

void ShortcutMenuHandlingSubManager::StoreShortcutMenuData(
    const webapps::AppId& app_id,
    std::vector<WebAppShortcutsMenuItemInfo> shortcut_menu_item_info,
    proto::ShortcutMenus* shortcut_menus,
    WebAppIconManager::ShortcutIconDataVector downloaded_shortcut_menu_items) {
  MeasureShortcutMenuIconHistograms(downloaded_shortcut_menu_items.size(),
                                    shortcut_menu_item_info.size());

  // Due to the bitmaps possibly being not populated (see
  // https://crbug.com/1427444), we just have empty bitmap data in that case. We
  // continue to check to make sure that there aren't MORE bitmaps than
  // items.
  while (downloaded_shortcut_menu_items.size() <
         shortcut_menu_item_info.size()) {
    downloaded_shortcut_menu_items.emplace_back();
  }
  for (size_t menu_index = 0; menu_index < shortcut_menu_item_info.size();
       menu_index++) {
    proto::ShortcutMenuInfo* new_shortcut_menu_item =
        shortcut_menus->add_shortcut_menu_info();
    new_shortcut_menu_item->set_shortcut_name(
        base::UTF16ToUTF8(shortcut_menu_item_info[menu_index].name));
    new_shortcut_menu_item->set_shortcut_launch_url(
        shortcut_menu_item_info[menu_index].url.spec());

    for (const auto& [size, time] :
         downloaded_shortcut_menu_items[menu_index][IconPurpose::ANY]) {
      proto::ShortcutIconData* icon_data =
          new_shortcut_menu_item->add_icon_data_any();
      icon_data->set_icon_size(size);
      icon_data->set_timestamp(syncer::TimeToProtoTime(time));
    }

    for (const auto& [size, time] :
         downloaded_shortcut_menu_items[menu_index][IconPurpose::MASKABLE]) {
      proto::ShortcutIconData* icon_data =
          new_shortcut_menu_item->add_icon_data_maskable();
      icon_data->set_icon_size(size);
      icon_data->set_timestamp(syncer::TimeToProtoTime(time));
    }

    for (const auto& [size, time] :
         downloaded_shortcut_menu_items[menu_index][IconPurpose::MONOCHROME]) {
      proto::ShortcutIconData* icon_data =
          new_shortcut_menu_item->add_icon_data_monochrome();
      icon_data->set_icon_size(size);
      icon_data->set_timestamp(syncer::TimeToProtoTime(time));
    }
  }
}

void ShortcutMenuHandlingSubManager::StartShortcutsMenuUnregistration(
    const webapps::AppId& app_id,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure registration_callback) {
  if (!HasShortcutsMenuInfo(current_state)) {
    std::move(registration_callback).Run();
    return;
  }

  web_app::UnregisterShortcutsMenuWithOs(
      app_id, profile_path_, base::BindOnce([](Result result) {
                               base::UmaHistogramBoolean(
                                   "WebApp.ShortcutsMenuUnregistered.Result",
                                   (result == Result::kOk));
                             }).Then(std::move(registration_callback)));
}

void ShortcutMenuHandlingSubManager::ReadIconDataForShortcutsMenu(
    const webapps::AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure execute_complete) {
  if (!HasShortcutsMenuInfo(desired_state)) {
    std::move(execute_complete).Run();
    return;
  }

  provider_->icon_manager().ReadAllShortcutsMenuIcons(
      app_id, base::BindOnce(&ShortcutMenuHandlingSubManager::
                                 OnIconDataLoadedRegisterShortcutsMenu,
                             weak_ptr_factory_.GetWeakPtr(), app_id,
                             desired_state, std::move(execute_complete)));
}

void ShortcutMenuHandlingSubManager::OnIconDataLoadedRegisterShortcutsMenu(
    const webapps::AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure execute_complete,
    ShortcutsMenuIconBitmaps shortcut_menu_icon_bitmaps) {
  base::FilePath shortcut_data_dir = GetOsIntegrationResourcesDirectoryForApp(
      profile_path_, app_id,
      provider_->registrar_unsafe().GetAppStartUrl(app_id));
  web_app::RegisterShortcutsMenuWithOs(
      app_id, profile_path_, shortcut_data_dir,
      CreateShortcutsMenuItemInfos(desired_state.shortcut_menus()),
      shortcut_menu_icon_bitmaps,
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.ShortcutsMenuRegistration.Result",
                                  (result == Result::kOk));
      }).Then(std::move(execute_complete)));
}

}  // namespace web_app
