// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/settings_window_manager_chromeos.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/window_properties.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace chrome {

namespace {

bool g_force_deprecated_settings_window_for_testing = false;
SettingsWindowManager* g_instance = nullptr;

class LegacySettingsTitleUpdater : public aura::WindowTracker {
 public:
  LegacySettingsTitleUpdater() = default;
  LegacySettingsTitleUpdater(const LegacySettingsTitleUpdater&) = delete;
  LegacySettingsTitleUpdater& operator=(const LegacySettingsTitleUpdater&) =
      delete;
  ~LegacySettingsTitleUpdater() override = default;

  // aura::WindowTracker:
  void OnWindowTitleChanged(aura::Window* window) override {
    // Name the window "Settings" instead of "Google Chrome - Settings".
    window->SetTitle(l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
  }
};

}  // namespace

SettingsWindowManager::SettingsWindowManager()
    : legacy_settings_title_updater_(
          std::make_unique<LegacySettingsTitleUpdater>()) {
  CHECK(!g_instance);
  g_instance = this;
}

SettingsWindowManager::~SettingsWindowManager() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
SettingsWindowManager* SettingsWindowManager::GetInstance() {
  return g_instance;
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
  if (IsRunningInForcedAppMode()) {
    return true;
  }

  return !web_app::AreWebAppsEnabled(profile);
}

void SettingsWindowManager::Open(const user_manager::User& user,
                                 OpenParams params) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(&user));

  // TODO(crbug.com/47287122): unify SettingsWindowManager::ShowOSSettings,
  // after callers are updated.
  if (params.setting_id.has_value()) {
    ShowOSSettings(profile, params.sub_page, *params.setting_id,
                   params.display_id);
  } else {
    ShowOSSettings(profile, params.sub_page, params.display_id);
  }
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
    params.window_action = NavigateParams::WindowAction::kShowWindow;
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
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.user_gesture = true;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  Navigate(&params);
  CHECK(params.browser);  // See https://crbug.com/1174525
  browser = params.browser->GetBrowserForMigrationOnly();

  // operator[] not used because SessionID has no default constructor.
  settings_session_map_.emplace(profile, SessionID::InvalidValue())
      .first->second = browser->session_id();
  DCHECK(browser->is_trusted_source());

  // Configure the created window property.
  auto* window = browser->window()->GetNativeWindow();
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  window->SetProperty(ash::kOverrideWindowIconResourceIdKey,
                      IDR_SETTINGS_LOGO_192);
  window->SetTitle(l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
  const ash::ShelfID shelf_id(ash::kInternalAppIdSettings);
  window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(ash::kAppIDKey, shelf_id.app_id);
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
  legacy_settings_title_updater_->Add(window);

  if (callback) {
    std::move(callback).Run(apps::LaunchResult(apps::State::kSuccess));
  }
}

void SettingsWindowManager::ShowOSSettings(Profile* profile,
                                           int64_t display_id) {
  ShowOSSettings(profile, std::string(), display_id);
}

void SettingsWindowManager::ShowOSSettings(Profile* profile,
                                           std::string_view sub_page,
                                           int64_t display_id) {
  ShowChromePageForProfile(profile, chrome::GetOSSettingsUrl(sub_page),
                           display_id, /*callback=*/{});
}

void SettingsWindowManager::ShowOSSettings(
    Profile* profile,
    std::string_view sub_page,
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
  if (iter != settings_session_map_.end()) {
    return chrome::FindBrowserWithID(iter->second);
  }

  return nullptr;
}

bool SettingsWindowManager::IsSettingsBrowser(
    BrowserWindowInterface* browser) const {
  DCHECK(browser);

  Profile* const profile = browser->GetProfile();
  if (!UseDeprecatedSettingsWindow(profile)) {
    const web_app::AppBrowserController* const app_controller =
        web_app::AppBrowserController::From(browser);
    if (!app_controller) {
      return false;
    }

    // TODO(calamity): Determine whether, during startup, we need to wait for
    // app install and then provide a valid answer here.
    std::optional<std::string> settings_app_id =
        ash::GetAppIdForSystemWebApp(profile, ash::SystemWebAppType::SETTINGS);
    return settings_app_id &&
           app_controller->app_id() == settings_app_id.value();
  }

  auto iter = settings_session_map_.find(profile);
  return iter != settings_session_map_.end() &&
         iter->second == browser->GetSessionID();
}

}  // namespace chrome
