// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"

#include <algorithm>

#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/extension_app_utils.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "chrome/browser/ash/browser_delegate/browser_type.h"
#include "chrome/browser/ash/eche_app/app_id.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_item_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

const extensions::Extension* GetExtensionForAppID(const std::string& app_id,
                                                  Profile* profile) {
  return extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
      app_id, extensions::ExtensionRegistry::EVERYTHING);
}

AppListControllerDelegate::Pinnable GetPinnableForAppID(
    const std::string& app_id,
    Profile* profile) {
  // These file manager apps have a shelf presence, but can only be launched
  // when provided a filename to open. Likewise, the feedback extension, the
  // Eche application need context when launching. Pinning these creates an
  // item that does nothing.
  const char* kNoPinAppIds[] = {
      ash::eche_app::kEcheAppId,
  };
  if (base::Contains(kNoPinAppIds, app_id)) {
    return AppListControllerDelegate::NO_PIN;
  }

  const std::optional<std::vector<std::string>> policy_ids =
      apps_util::GetPolicyIdsFromAppId(profile, app_id);

  if (!policy_ids || policy_ids->empty()) {
    return AppListControllerDelegate::PIN_EDITABLE;
  }

  if (ash::DemoSession::Get() &&
      std::ranges::none_of(*policy_ids, [](const auto& policy_id) {
        return ash::DemoSession::Get()->ShouldShowAppInShelf(policy_id);
      })) {
    return AppListControllerDelegate::PIN_EDITABLE;
  }

  const base::Value::List& policy_apps =
      profile->GetPrefs()->GetList(prefs::kPolicyPinnedLauncherApps);

  for (const base::Value& policy_dict_entry : policy_apps) {
    if (!policy_dict_entry.is_dict()) {
      return AppListControllerDelegate::PIN_EDITABLE;
    }

    const std::string* policy_entry = policy_dict_entry.GetDict().FindString(
        ChromeShelfPrefs::kPinnedAppsPrefAppIDKey);
    if (!policy_entry) {
      return AppListControllerDelegate::PIN_EDITABLE;
    }

    if (base::Contains(*policy_ids,
                       apps_util::TransformRawPolicyId(*policy_entry))) {
      return AppListControllerDelegate::PIN_FIXED;
    }
  }

  return AppListControllerDelegate::PIN_EDITABLE;
}

bool IsAppHiddenFromShelf(Profile* profile, const std::string& app_id) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }

  bool hidden = false;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&hidden](const apps::AppUpdate& update) {
        hidden = !update.ShowInShelf().value_or(true);
      });

  return hidden;
}

bool IsPromiseAppReadyToShowInShelf(Profile* profile,
                                    const std::string& promise_package_id) {
  CHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  const apps::PromiseApp* promise_app =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->PromiseAppRegistryCache()
          ->GetPromiseAppForStringPackageId(promise_package_id);
  return promise_app && promise_app->should_show.value_or(false);
}

bool IsAppPinEditable(apps::AppType app_type,
                      const std::string& app_id,
                      Profile* profile) {
  if (apps::AppServiceProxyFactory::GetForProfile(profile)
          ->PromiseAppRegistryCache()
          ->GetPromiseAppForStringPackageId(app_id)) {
    return true;
  }

  if (IsAppHiddenFromShelf(profile, app_id)) {
    return false;
  }

  switch (app_type) {
    case apps::AppType::kArc: {
      const arc::ArcAppShelfId& arc_shelf_id =
          arc::ArcAppShelfId::FromString(app_id);
      DCHECK(arc_shelf_id.valid());
      const ArcAppListPrefs* arc_list_prefs = ArcAppListPrefs::Get(profile);
      DCHECK(arc_list_prefs);
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          arc_list_prefs->GetApp(arc_shelf_id.app_id());
      if (!arc_shelf_id.has_shelf_group_id() && app_info &&
          app_info->launchable) {
        return true;
      }
      return false;
    }
    case apps::AppType::kPluginVm: {
      bool show_in_launcher = false;
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->AppRegistryCache()
          .ForOneApp(
              app_id, [&show_in_launcher](const apps::AppUpdate& update) {
                show_in_launcher = update.ShowInLauncher().value_or(false);
              });
      return show_in_launcher;
    }
    case apps::AppType::kCrostini:
    case apps::AppType::kBorealis:
    case apps::AppType::kChromeApp:
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
      return true;
    case apps::AppType::kUnknown:
      // Type kUnknown is used for "unregistered" Crostini apps, which do not
      // have a .desktop file and can only be closed, not pinned.
      return false;
    case apps::AppType::kRemote:
    case apps::AppType::kExtension:
      NOTREACHED() << "Type " << (int)app_type
                   << " should not appear in shelf.";
    case apps::AppType::kBruschetta:
      return true;
  }
}

const AccountId& GetActiveAccountId() {
  const session_manager::Session& active_session =
      CHECK_DEREF(session_manager::SessionManager::Get()->GetActiveSession());
  auto& account_id = active_session.account_id();
  CHECK(!account_id.empty());
  return account_id;
}

bool IsAppBrowser(const ash::BrowserDelegate& browser) {
  auto type = browser.GetType();
  return type == ash::BrowserType::kApp || type == ash::BrowserType::kAppPopup;
}

bool IsBrowserRepresentedInBrowserList(const ash::BrowserDelegate* browser,
                                       const ash::ShelfModel* model) {
  // Only browser windows for the active user are represented.
  if (!browser || browser->GetAccountId() != GetActiveAccountId()) {
    return false;
  }

  // V1 App popup windows may have their own item.
  const bool is_app = browser->GetType() == ash::BrowserType::kApp ||
                      browser->GetType() == ash::BrowserType::kAppPopup;
  if (is_app && model->ItemByID(ash::ShelfID(
                    browser->GetAppId().value_or(std::string())))) {
    return false;
  }

  return true;
}

void ShowAndActivateBrowser(bool move_to_current_desktop,
                            ash::BrowserDelegate* browser) {
  if (move_to_current_desktop) {
    multi_user_util::MoveWindowToCurrentDesktop(browser->GetNativeWindow());
  }
  browser->Show();
  browser->Activate();
}

void PinAppWithIDToShelf(const std::string& app_id) {
  auto* shelf_controller = ChromeShelfController::instance();
  auto* shelf_model = shelf_controller->shelf_model();
  if (shelf_model->ItemIndexByAppID(app_id) >= 0) {
    shelf_model->PinExistingItemWithID(app_id);
  } else {
    shelf_model->AddAndPinAppWithFactoryConstructedDelegate(app_id);
  }
}

void UnpinAppWithIDFromShelf(const std::string& app_id) {
  auto* shelf_controller = ChromeShelfController::instance();
  shelf_controller->shelf_model()->UnpinAppWithID(app_id);
}

apps::LaunchSource ShelfLaunchSourceToAppsLaunchSource(
    ash::ShelfLaunchSource source) {
  switch (source) {
    case ash::LAUNCH_FROM_UNKNOWN:
      return apps::LaunchSource::kUnknown;
    case ash::LAUNCH_FROM_INTERNAL:
      return apps::LaunchSource::kFromChromeInternal;
    case ash::LAUNCH_FROM_APP_LIST:
      return apps::LaunchSource::kFromAppListGrid;
    case ash::LAUNCH_FROM_APP_LIST_SEARCH:
      return apps::LaunchSource::kFromAppListQuery;
    case ash::LAUNCH_FROM_APP_LIST_RECOMMENDATION:
      return apps::LaunchSource::kFromAppListRecommendation;
    case ash::LAUNCH_FROM_SHELF:
      return apps::LaunchSource::kFromShelf;
  }
}
