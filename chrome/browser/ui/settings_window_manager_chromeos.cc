// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/settings_window_manager_chromeos.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/window_properties.h"
#include "base/strings/strcat.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_observer_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace chrome {

namespace {

bool g_force_deprecated_settings_window_for_testing = false;
SettingsWindowManager* g_settings_window_manager_for_testing = nullptr;

}  // namespace

// static
SettingsWindowManager* SettingsWindowManager::GetInstance() {
  return g_settings_window_manager_for_testing
             ? g_settings_window_manager_for_testing
             : base::Singleton<SettingsWindowManager>::get();
}

// static
void SettingsWindowManager::SetInstanceForTesting(
    SettingsWindowManager* manager) {
  g_settings_window_manager_for_testing = manager;
}

// static
void SettingsWindowManager::ForceDeprecatedSettingsWindowForTesting() {
  g_force_deprecated_settings_window_for_testing = true;
}

// static
bool SettingsWindowManager::UseDeprecatedSettingsWindow(Profile* profile) {
  if (g_force_deprecated_settings_window_for_testing) {
    return true;
  }

  // Use deprecated settings window in Kiosk session only if SWA is disabled.
  if (IsRunningInForcedAppMode() &&
      !base::FeatureList::IsEnabled(ash::features::kKioskEnableSystemWebApps)) {
    return true;
  }

  return !web_app::AreWebAppsEnabled(profile);
}

void SettingsWindowManager::AddObserver(
    SettingsWindowManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void SettingsWindowManager::RemoveObserver(
    SettingsWindowManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SettingsWindowManager::ShowChromePageForProfile(
    Profile* profile,
    const GURL& gurl,
    int64_t display_id,
    apps::LaunchCallback callback) {
  // Use the original (non off-the-record) profile for settings unless
  // this is a guest session.
  if (!profile->IsGuestSession() && profile->IsOffTheRecord()) {
    profile = profile->GetOriginalProfile();
  }

  // If this profile isn't allowed to create browser windows (e.g. the login
  // screen profile) then bail out. Neither the new SWA code path nor the legacy
  // code path can successfully open the window for these profiles.
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    LOG(ERROR) << "Unable to open settings for this profile, url "
               << gurl.spec();
    if (callback) {
      std::move(callback).Run(apps::LaunchResult(apps::State::kFailed));
    }
    return;
  }

  // TODO(crbug.com/1067073): Remove legacy Settings Window.
  if (!UseDeprecatedSettingsWindow(profile)) {
    ash::SystemAppLaunchParams params;
    params.url = gurl;
    ash::LaunchSystemWebAppAsync(
        profile, ash::SystemWebAppType::SETTINGS, params,
        std::make_unique<apps::WindowInfo>(display_id),
        callback ? std::make_optional(std::move(callback)) : std::nullopt);
    // SWA OS Settings don't use SettingsWindowManager to manage windows, don't
    // notify SettingsWindowObservers.
    return;
  }

  // Look for an existing browser window.
  Browser* browser = FindBrowserForProfile(profile);
  if (browser) {
    DCHECK(browser->profile() == profile);
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetWebContentsAt(0);
    if (web_contents && web_contents->GetURL() == gurl) {
      browser->window()->Show();
      if (callback) {
        std::move(callback).Run(apps::LaunchResult(apps::State::kSuccess));
      }
      return;
    }

    NavigateParams params(browser, gurl, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.window_action = NavigateParams::SHOW_WINDOW;
    params.user_gesture = true;
    Navigate(&params);
    if (callback) {
      std::move(callback).Run(apps::LaunchResult(apps::State::kSuccess));
    }
    return;
  }

  // No existing browser window, create one.
  NavigateParams params(profile, gurl, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.trusted_source = true;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);
  browser = params.browser;
  CHECK(browser);  // See https://crbug.com/1174525

  // operator[] not used because SessionID has no default constructor.
  settings_session_map_.emplace(profile, SessionID::InvalidValue())
      .first->second = browser->session_id();
  DCHECK(browser->is_trusted_source());

  auto* window = browser->window()->GetNativeWindow();
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  window->SetProperty(ash::kOverrideWindowIconResourceIdKey,
                      IDR_SETTINGS_LOGO_192);

  for (SettingsWindowManagerObserver& observer : observers_) {
    observer.OnNewSettingsWindow(browser);
  }

  if (callback) {
    std::move(callback).Run(apps::LaunchResult(apps::State::kSuccess));
  }
}

void SettingsWindowManager::ShowOSSettings(Profile* profile,
                                           int64_t display_id) {
  ShowOSSettings(profile, std::string(), display_id);
}

void SettingsWindowManager::ShowOSSettings(Profile* profile,
                                           const std::string& sub_page,
                                           int64_t display_id) {
  ShowChromePageForProfile(profile, chrome::GetOSSettingsUrl(sub_page),
                           display_id, /*callback=*/{});
}

void SettingsWindowManager::ShowOSSettings(
    Profile* profile,
    const std::string& sub_page,
    const chromeos::settings::mojom::Setting setting_id,
    int64_t display_id) {
  std::string path_with_setting_id =
      base::StrCat({sub_page, std::string("?settingId="),
                    base::NumberToString(base::to_underlying(setting_id))});

  ShowOSSettings(profile, path_with_setting_id, display_id);
}

Browser* SettingsWindowManager::FindBrowserForProfile(Profile* profile) {
  if (!UseDeprecatedSettingsWindow(profile)) {
    return ash::FindSystemWebAppBrowser(profile,
                                        ash::SystemWebAppType::SETTINGS);
  }

  auto iter = settings_session_map_.find(profile);
  if (iter != settings_session_map_.end())
    return chrome::FindBrowserWithID(iter->second);

  return nullptr;
}

bool SettingsWindowManager::IsSettingsBrowser(Browser* browser) const {
  DCHECK(browser);

  Profile* profile = browser->profile();
  if (!UseDeprecatedSettingsWindow(profile)) {
    if (!browser->app_controller())
      return false;

    // TODO(calamity): Determine whether, during startup, we need to wait for
    // app install and then provide a valid answer here.
    std::optional<std::string> settings_app_id =
        ash::GetAppIdForSystemWebApp(profile, ash::SystemWebAppType::SETTINGS);
    return settings_app_id &&
           browser->app_controller()->app_id() == settings_app_id.value();
  } else {
    auto iter = settings_session_map_.find(profile);
    return iter != settings_session_map_.end() &&
           iter->second == browser->session_id();
  }
}

SettingsWindowManager::SettingsWindowManager() {}

SettingsWindowManager::~SettingsWindowManager() {}

}  // namespace chrome
