// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/ash/kiosk_apps_handler.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/crx_file/id_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "extensions/common/extension_urls.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Returns a Value::Dict populated with `app_data`.
base::Value::Dict PopulateAppDict(const KioskAppManager::App& app_data) {
  std::string icon_url;
  if (app_data.icon.isNull()) {
    icon_url = webui::GetBitmapDataUrl(*ui::ResourceBundle::GetSharedInstance()
                                            .GetImageNamed(IDR_APP_DEFAULT_ICON)
                                            .ToSkBitmap());
  } else {
    icon_url = webui::GetBitmapDataUrl(*app_data.icon.bitmap());
  }
  base::Value::Dict app_dict;
  // The items which are to be written into app_dict are also described in
  // chrome/browser/resources/extensions/chromeos/kiosk_app_list.js in @typedef
  // for AppDict. Please update it whenever you add or remove any keys here.
  app_dict.Set("id", app_data.app_id);
  app_dict.Set("name", app_data.name);
  app_dict.Set("iconURL", icon_url);
  app_dict.Set("autoLaunch",
               KioskAppManager::Get()->GetAutoLaunchApp() == app_data.app_id &&
                   (KioskAppManager::Get()->IsAutoLaunchEnabled() ||
                    KioskAppManager::Get()->IsAutoLaunchRequested()));
  app_dict.Set("isLoading", app_data.is_loading);
  return app_dict;
}

// Sanitize app id input value and extracts app id out of it.
// Returns false if an app id could not be derived out of the input.
bool ExtractsAppIdFromInput(const std::string& input,
                            std::string* app_id) {
  if (crx_file::id_util::IdIsValid(input)) {
    *app_id = input;
    return true;
  }

  GURL webstore_url = GURL(input);
  if (!webstore_url.is_valid())
    return false;

  GURL webstore_base_url =
      GURL(extension_urls::GetWebstoreItemDetailURLPrefix());

  if (webstore_url.scheme() != webstore_base_url.scheme() ||
      webstore_url.host() != webstore_base_url.host() ||
      !base::StartsWith(webstore_url.path(), webstore_base_url.path(),
                        base::CompareCase::SENSITIVE)) {
    return false;
  }

  const std::string path = webstore_url.path();
  const size_t last_slash = path.rfind('/');
  if (last_slash == std::string::npos)
    return false;

  const std::string candidate_id = path.substr(last_slash + 1);
  if (!crx_file::id_util::IdIsValid(candidate_id))
    return false;

  *app_id = candidate_id;
  return true;
}

}  // namespace

KioskAppsHandler::KioskAppsHandler(OwnerSettingsServiceAsh* service)
    : kiosk_app_manager_(KioskAppManager::Get()),
      initialized_(false),
      is_kiosk_enabled_(false),
      is_auto_launch_enabled_(false),
      owner_settings_service_(service) {}

KioskAppsHandler::~KioskAppsHandler() {
  // TODO(tommycli): This is needed because OnJavascriptDisallowed only called
  // with refresh or off-page navigation, otherwise DCHECK triggered when
  // exiting.
  kiosk_app_manager_->RemoveObserver(this);
}

void KioskAppsHandler::OnJavascriptAllowed() {
  kiosk_app_manager_->AddObserver(this);
}

void KioskAppsHandler::OnJavascriptDisallowed() {
  kiosk_app_manager_->RemoveObserver(this);
}

void KioskAppsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeKioskAppSettings",
      base::BindRepeating(&KioskAppsHandler::HandleInitializeKioskAppSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getKioskAppSettings",
      base::BindRepeating(&KioskAppsHandler::HandleGetKioskAppSettings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addKioskApp", base::BindRepeating(&KioskAppsHandler::HandleAddKioskApp,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "removeKioskApp",
      base::BindRepeating(&KioskAppsHandler::HandleRemoveKioskApp,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "enableKioskAutoLaunch",
      base::BindRepeating(&KioskAppsHandler::HandleEnableKioskAutoLaunch,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "disableKioskAutoLaunch",
      base::BindRepeating(&KioskAppsHandler::HandleDisableKioskAutoLaunch,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDisableBailoutShortcut",
      base::BindRepeating(&KioskAppsHandler::HandleSetDisableBailoutShortcut,
                          base::Unretained(this)));
}

void KioskAppsHandler::OnKioskAppDataChanged(const std::string& app_id) {
  UpdateApp(app_id);
}

void KioskAppsHandler::OnKioskAppDataLoadFailure(const std::string& app_id) {
  ShowError(app_id);
}

void KioskAppsHandler::OnKioskExtensionLoadedInCache(
    const std::string& app_id) {
  UpdateApp(app_id);
}

void KioskAppsHandler::OnKioskExtensionDownloadFailed(
    const std::string& app_id) {
  ShowError(app_id);
}

void KioskAppsHandler::OnGetConsumerKioskAutoLaunchStatus(
    const std::string& callback_id,
    ash::KioskAppManager::ConsumerKioskAutoLaunchStatus status) {
  initialized_ = true;
  if (KioskAppManager::IsConsumerKioskEnabled()) {
    if (!base::SysInfo::IsRunningOnChromeOS()) {
      // Enable everything when running on a dev box.
      is_kiosk_enabled_ = true;
      is_auto_launch_enabled_ = true;
    } else {
      // Enable consumer kiosk for owner and enable auto launch if configured.
      is_kiosk_enabled_ =
          ProfileHelper::IsOwnerProfile(Profile::FromWebUI(web_ui()));
      is_auto_launch_enabled_ =
          status == KioskAppManager::ConsumerKioskAutoLaunchStatus::kEnabled;
    }
  } else {
    // Otherwise, consumer kiosk is disabled.
    is_kiosk_enabled_ = false;
    is_auto_launch_enabled_ = false;
  }

  base::Value::Dict kiosk_params;
  kiosk_params.Set("kioskEnabled", is_kiosk_enabled_);
  kiosk_params.Set("autoLaunchEnabled", is_auto_launch_enabled_);
  ResolveJavascriptCallback(base::Value(callback_id), kiosk_params);
}

void KioskAppsHandler::OnKioskAppsSettingsChanged() {
  FireWebUIListener("kiosk-app-settings-changed", GetSettingsDictionary());
}

base::Value::Dict KioskAppsHandler::GetSettingsDictionary() {
  base::Value::Dict settings;
  if (!initialized_ || !is_kiosk_enabled_) {
    return settings;
  }

  bool enable_bailout_shortcut;
  if (!CrosSettings::Get()->GetBoolean(
          kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
          &enable_bailout_shortcut)) {
    enable_bailout_shortcut = true;
  }

  settings.Set("disableBailout", !enable_bailout_shortcut);
  settings.Set("hasAutoLaunchApp",
               !kiosk_app_manager_->GetAutoLaunchApp().empty());

  KioskAppManager::AppList apps;
  kiosk_app_manager_->GetApps(&apps);

  base::Value::List apps_list;
  for (size_t i = 0; i < apps.size(); ++i) {
    const KioskAppManager::App& app_data = apps[i];
    apps_list.Append(PopulateAppDict(app_data));
  }
  settings.Set("apps", std::move(apps_list));

  return settings;
}

void KioskAppsHandler::HandleInitializeKioskAppSettings(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args.front().GetString();

  AllowJavascript();
  KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
      base::BindOnce(&KioskAppsHandler::OnGetConsumerKioskAutoLaunchStatus,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void KioskAppsHandler::HandleGetKioskAppSettings(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args.front().GetString();

  ResolveJavascriptCallback(base::Value(callback_id), GetSettingsDictionary());
}

void KioskAppsHandler::HandleAddKioskApp(const base::Value::List& args) {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  const std::string& input = args.front().GetString();

  std::string app_id;
  if (!ExtractsAppIdFromInput(input, &app_id)) {
    OnKioskAppDataLoadFailure(input);
    return;
  }

  kiosk_app_manager_->AddApp(app_id, owner_settings_service_);
}

void KioskAppsHandler::HandleRemoveKioskApp(const base::Value::List& args) {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  const std::string& app_id = args.front().GetString();

  kiosk_app_manager_->RemoveApp(app_id, owner_settings_service_);
}

void KioskAppsHandler::HandleEnableKioskAutoLaunch(
    const base::Value::List& args) {
  if (!initialized_ || !is_kiosk_enabled_ || !is_auto_launch_enabled_)
    return;

  const std::string& app_id = args.front().GetString();

  kiosk_app_manager_->SetAutoLaunchApp(app_id, owner_settings_service_);
}

void KioskAppsHandler::HandleDisableKioskAutoLaunch(
    const base::Value::List& args) {
  if (!initialized_ || !is_kiosk_enabled_ || !is_auto_launch_enabled_)
    return;

  const std::string& app_id = args.front().GetString();

  std::string startup_app_id = kiosk_app_manager_->GetAutoLaunchApp();
  if (startup_app_id != app_id)
    return;

  kiosk_app_manager_->SetAutoLaunchApp("", owner_settings_service_);
}

void KioskAppsHandler::HandleSetDisableBailoutShortcut(
    const base::Value::List& args) {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  CHECK(!args.empty());
  const bool disable_bailout_shortcut = args.front().GetBool();

  owner_settings_service_->SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
      !disable_bailout_shortcut);
}

void KioskAppsHandler::UpdateApp(const std::string& app_id) {
  KioskAppManager::App app_data;
  if (!kiosk_app_manager_->GetApp(app_id, &app_data))
    return;

  FireWebUIListener("kiosk-app-updated", PopulateAppDict(app_data));
}

void KioskAppsHandler::ShowError(const std::string& app_id) {
  base::Value app_id_value(app_id);
  FireWebUIListener("kiosk-app-error", app_id_value);

  kiosk_app_manager_->RemoveApp(app_id, owner_settings_service_);
}

}  // namespace ash
