// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/chromeos/kiosk_apps_handler.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
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

namespace chromeos {

namespace {

// Populates app info dictionary with |app_data|.
void PopulateAppDict(const KioskAppManager::App& app_data,
                     base::DictionaryValue* app_dict) {
  std::string icon_url;
  if (app_data.icon.isNull()) {
    icon_url = webui::GetBitmapDataUrl(*ui::ResourceBundle::GetSharedInstance()
                                            .GetImageNamed(IDR_APP_DEFAULT_ICON)
                                            .ToSkBitmap());
  } else {
    icon_url = webui::GetBitmapDataUrl(*app_data.icon.bitmap());
  }

  // The items which are to be written into app_dict are also described in
  // chrome/browser/resources/extensions/chromeos/kiosk_app_list.js in @typedef
  // for AppDict. Please update it whenever you add or remove any keys here.
  app_dict->SetString("id", app_data.app_id);
  app_dict->SetString("name", app_data.name);
  app_dict->SetString("iconURL", icon_url);
  app_dict->SetBoolean(
      "autoLaunch",
      KioskAppManager::Get()->GetAutoLaunchApp() == app_data.app_id &&
      (KioskAppManager::Get()->IsAutoLaunchEnabled() ||
          KioskAppManager::Get()->IsAutoLaunchRequested()));
  app_dict->SetBoolean("isLoading", app_data.is_loading);
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

KioskAppsHandler::KioskAppsHandler(OwnerSettingsServiceChromeOS* service)
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
    chromeos::KioskAppManager::ConsumerKioskAutoLaunchStatus status) {
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
          status == KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_ENABLED;
    }
  } else {
    // Otherwise, consumer kiosk is disabled.
    is_kiosk_enabled_ = false;
    is_auto_launch_enabled_ = false;
  }

  base::DictionaryValue kiosk_params;
  kiosk_params.SetBoolean("kioskEnabled", is_kiosk_enabled_);
  kiosk_params.SetBoolean("autoLaunchEnabled", is_auto_launch_enabled_);
  ResolveJavascriptCallback(base::Value(callback_id), kiosk_params);
}


void KioskAppsHandler::OnKioskAppsSettingsChanged() {
  FireWebUIListener("kiosk-app-settings-changed", *GetSettingsDictionary());
}

std::unique_ptr<base::DictionaryValue>
KioskAppsHandler::GetSettingsDictionary() {
  std::unique_ptr<base::DictionaryValue> settings(new base::DictionaryValue);

  if (!initialized_ || !is_kiosk_enabled_) {
    return settings;
  }

  bool enable_bailout_shortcut;
  if (!CrosSettings::Get()->GetBoolean(
          kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
          &enable_bailout_shortcut)) {
    enable_bailout_shortcut = true;
  }

  settings->SetBoolean("disableBailout", !enable_bailout_shortcut);
  settings->SetBoolean("hasAutoLaunchApp",
                       !kiosk_app_manager_->GetAutoLaunchApp().empty());

  KioskAppManager::AppList apps;
  kiosk_app_manager_->GetApps(&apps);

  std::unique_ptr<base::ListValue> apps_list(new base::ListValue);
  for (size_t i = 0; i < apps.size(); ++i) {
    const KioskAppManager::App& app_data = apps[i];

    std::unique_ptr<base::DictionaryValue> app_info(new base::DictionaryValue);
    PopulateAppDict(app_data, app_info.get());
    apps_list->Append(std::move(app_info));
  }
  settings->SetWithoutPathExpansion("apps", std::move(apps_list));

  return settings;
}

void KioskAppsHandler::HandleInitializeKioskAppSettings(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  AllowJavascript();
  KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
      base::Bind(&KioskAppsHandler::OnGetConsumerKioskAutoLaunchStatus,
                 weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void KioskAppsHandler::HandleGetKioskAppSettings(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  ResolveJavascriptCallback(base::Value(callback_id), *GetSettingsDictionary());
}


void KioskAppsHandler::HandleAddKioskApp(const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  std::string input;
  CHECK(args->GetString(0, &input));

  std::string app_id;
  if (!ExtractsAppIdFromInput(input, &app_id)) {
    OnKioskAppDataLoadFailure(input);
    return;
  }

  kiosk_app_manager_->AddApp(app_id, owner_settings_service_);
}

void KioskAppsHandler::HandleRemoveKioskApp(const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  kiosk_app_manager_->RemoveApp(app_id, owner_settings_service_);
}

void KioskAppsHandler::HandleEnableKioskAutoLaunch(
    const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_ || !is_auto_launch_enabled_)
    return;

  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  kiosk_app_manager_->SetAutoLaunchApp(app_id, owner_settings_service_);
}

void KioskAppsHandler::HandleDisableKioskAutoLaunch(
    const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_ || !is_auto_launch_enabled_)
    return;

  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  std::string startup_app_id = kiosk_app_manager_->GetAutoLaunchApp();
  if (startup_app_id != app_id)
    return;

  kiosk_app_manager_->SetAutoLaunchApp("", owner_settings_service_);
}

void KioskAppsHandler::HandleSetDisableBailoutShortcut(
    const base::ListValue* args) {
  if (!initialized_ || !is_kiosk_enabled_)
    return;

  bool disable_bailout_shortcut;
  CHECK(args->GetBoolean(0, &disable_bailout_shortcut));

  owner_settings_service_->SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
      !disable_bailout_shortcut);
}

void KioskAppsHandler::UpdateApp(const std::string& app_id) {
  KioskAppManager::App app_data;
  if (!kiosk_app_manager_->GetApp(app_id, &app_data))
    return;

  base::DictionaryValue app_dict;
  PopulateAppDict(app_data, &app_dict);
  FireWebUIListener("kiosk-app-updated", app_dict);
}

void KioskAppsHandler::ShowError(const std::string& app_id) {
  base::Value app_id_value(app_id);
  FireWebUIListener("kiosk-app-error", app_id_value);

  kiosk_app_manager_->RemoveApp(app_id, owner_settings_service_);
}

}  // namespace chromeos
