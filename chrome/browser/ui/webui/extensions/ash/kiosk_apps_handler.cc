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
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

const SkBitmap& BitmapOrDefault(gfx::ImageSkia icon) {
  return icon.isNull() ? *ui::ResourceBundle::GetSharedInstance()
                              .GetImageNamed(IDR_APP_DEFAULT_ICON)
                              .ToSkBitmap()
                       : *icon.bitmap();
}

bool IsAutoLaunch(const KioskChromeAppManager::App& app) {
  return KioskChromeAppManager::Get()->GetAutoLaunchApp() == app.app_id &&
         (KioskChromeAppManager::Get()->IsAutoLaunchEnabled() ||
          KioskChromeAppManager::Get()->IsAutoLaunchRequested());
}

base::Value::Dict PopulateAppDict(const KioskChromeAppManager::App& app) {
  // This dict must mirror the `KioskApp` interface defined in
  // chrome/browser/resources/extensions/kiosk_browser_proxy.ts.
  return base::Value::Dict()
      .Set("id", app.app_id)
      .Set("name", app.name)
      .Set("iconURL", webui::GetBitmapDataUrl(BitmapOrDefault(app.icon)))
      .Set("autoLaunch", IsAutoLaunch(app))
      .Set("isLoading", app.is_loading);
}

base::Value::List ToAppDictList(
    const std::vector<KioskChromeAppManager::App>& apps) {
  base::Value::List list;
  for (const auto& app : apps) {
    list.Append(PopulateAppDict(app));
  }
  return list;
}

bool IsBailoutShortcutEnabled() {
  bool value_from_pref;
  if (CrosSettings::Get()->GetBoolean(
          kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
          &value_from_pref)) {
    return value_from_pref;
  }
  return true;
}

// Sanitize app id input value and extracts app id out of it.
// Returns false if an app id could not be derived out of the input.
bool ExtractsAppIdFromInput(const std::string& input, std::string* app_id) {
  if (crx_file::id_util::IdIsValid(input)) {
    *app_id = input;
    return true;
  }

  GURL webstore_url = GURL(input);
  if (!webstore_url.is_valid()) {
    return false;
  }

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
  if (last_slash == std::string::npos) {
    return false;
  }

  const std::string candidate_id = path.substr(last_slash + 1);
  if (!crx_file::id_util::IdIsValid(candidate_id)) {
    return false;
  }

  *app_id = candidate_id;
  return true;
}

}  // namespace

KioskAppsHandler::KioskAppsHandler(OwnerSettingsServiceAsh* service)
    : chrome_app_manager_(KioskChromeAppManager::Get()),
      initialized_(false),
      is_kiosk_enabled_(false),
      is_auto_launch_enabled_(false),
      owner_settings_service_(service) {}

KioskAppsHandler::~KioskAppsHandler() {
  // TODO(tommycli): This is needed because OnJavascriptDisallowed only called
  // with refresh or off-page navigation, otherwise DCHECK triggered when
  // exiting.
  chrome_app_manager_->RemoveObserver(this);
}

void KioskAppsHandler::OnJavascriptAllowed() {
  chrome_app_manager_->AddObserver(this);
}

void KioskAppsHandler::OnJavascriptDisallowed() {
  chrome_app_manager_->RemoveObserver(this);
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
    ash::KioskChromeAppManager::ConsumerKioskAutoLaunchStatus status) {
  initialized_ = true;
  if (KioskChromeAppManager::IsConsumerKioskEnabled()) {
    if (!base::SysInfo::IsRunningOnChromeOS()) {
      // Enable everything when running on a dev box.
      is_kiosk_enabled_ = true;
      is_auto_launch_enabled_ = true;
    } else {
      // Enable consumer kiosk for owner and enable auto launch if configured.
      is_kiosk_enabled_ =
          ProfileHelper::IsOwnerProfile(Profile::FromWebUI(web_ui()));
      is_auto_launch_enabled_ =
          status ==
          KioskChromeAppManager::ConsumerKioskAutoLaunchStatus::kEnabled;
    }
  } else {
    // Otherwise, consumer kiosk is disabled.
    is_kiosk_enabled_ = false;
    is_auto_launch_enabled_ = false;
  }

  auto kiosk_params = base::Value::Dict()
                          .Set("kioskEnabled", is_kiosk_enabled_)
                          .Set("autoLaunchEnabled", is_auto_launch_enabled_);
  ResolveJavascriptCallback(base::Value(callback_id), kiosk_params);
}

void KioskAppsHandler::OnKioskAppsSettingsChanged() {
  FireWebUIListener("kiosk-app-settings-changed", GetSettingsDictionary());
}

base::Value::Dict KioskAppsHandler::GetSettingsDictionary() {
  if (!initialized_ || !is_kiosk_enabled_) {
    return base::Value::Dict();
  }

  return base::Value::Dict()
      .Set("disableBailout", !IsBailoutShortcutEnabled())
      .Set("hasAutoLaunchApp", !chrome_app_manager_->GetAutoLaunchApp().empty())
      .Set("apps", ToAppDictList(chrome_app_manager_->GetApps()));
}

void KioskAppsHandler::HandleInitializeKioskAppSettings(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args.front().GetString();

  AllowJavascript();
  KioskChromeAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
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
  if (!initialized_ || !is_kiosk_enabled_) {
    return;
  }

  const std::string& input = args.front().GetString();

  std::string app_id;
  if (!ExtractsAppIdFromInput(input, &app_id)) {
    OnKioskAppDataLoadFailure(input);
    return;
  }

  chrome_app_manager_->AddApp(app_id, owner_settings_service_);
}

void KioskAppsHandler::HandleRemoveKioskApp(const base::Value::List& args) {
  if (!initialized_ || !is_kiosk_enabled_) {
    return;
  }

  const std::string& app_id = args.front().GetString();

  chrome_app_manager_->RemoveApp(app_id, owner_settings_service_);
}

void KioskAppsHandler::HandleEnableKioskAutoLaunch(
    const base::Value::List& args) {
  if (!initialized_ || !is_kiosk_enabled_ || !is_auto_launch_enabled_) {
    return;
  }

  const std::string& app_id = args.front().GetString();

  chrome_app_manager_->SetAutoLaunchApp(app_id, owner_settings_service_);
}

void KioskAppsHandler::HandleDisableKioskAutoLaunch(
    const base::Value::List& args) {
  if (!initialized_ || !is_kiosk_enabled_ || !is_auto_launch_enabled_) {
    return;
  }

  const std::string& app_id = args.front().GetString();

  std::string startup_app_id = chrome_app_manager_->GetAutoLaunchApp();
  if (startup_app_id != app_id) {
    return;
  }

  chrome_app_manager_->SetAutoLaunchApp("", owner_settings_service_);
}

void KioskAppsHandler::HandleSetDisableBailoutShortcut(
    const base::Value::List& args) {
  if (!initialized_ || !is_kiosk_enabled_) {
    return;
  }

  CHECK(!args.empty());
  const bool disable_bailout_shortcut = args.front().GetBool();

  owner_settings_service_->SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled,
      !disable_bailout_shortcut);
}

void KioskAppsHandler::UpdateApp(const std::string& app_id) {
  KioskChromeAppManager::App app_data;
  if (!chrome_app_manager_->GetApp(app_id, &app_data)) {
    return;
  }

  FireWebUIListener("kiosk-app-updated", PopulateAppDict(app_data));
}

void KioskAppsHandler::ShowError(const std::string& app_id) {
  base::Value app_id_value(app_id);
  FireWebUIListener("kiosk-app-error", app_id_value);

  chrome_app_manager_->RemoveApp(app_id, owner_settings_service_);
}

}  // namespace ash
