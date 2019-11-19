// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/settings_window_manager_chromeos.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_observer_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/aura_constants.h"
#include "url/gurl.h"

namespace chrome {

namespace {

// This method handles the case of resurfacing the user's OS Settings
// standalone window that may be at the time located on another user's desktop.
void ShowSettingsOnCurrentDesktop(Browser* browser) {
  auto* window_manager = MultiUserWindowManagerHelper::GetWindowManager();
  if (window_manager && browser) {
    window_manager->ShowWindowForUser(browser->window()->GetNativeWindow(),
                                      window_manager->CurrentAccountId());
    browser->window()->Show();
  }
}

}  // namespace

// static
SettingsWindowManager* SettingsWindowManager::GetInstance() {
  return base::Singleton<SettingsWindowManager>::get();
}

void SettingsWindowManager::AddObserver(
    SettingsWindowManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void SettingsWindowManager::RemoveObserver(
    SettingsWindowManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SettingsWindowManager::ShowChromePageForProfile(Profile* profile,
                                                     const GURL& gurl) {
  // Use the original (non off-the-record) profile for settings unless
  // this is a guest session.
  if (!profile->IsGuestSession() && profile->IsOffTheRecord())
    profile = profile->GetOriginalProfile();

  // TODO(calamity): Auto-launch the settings app on install if not found, and
  // figure out how to invoke OnNewSettingsWindow() in that case.
  if (web_app::SystemWebAppManager::IsEnabled()) {
    bool did_create;
    Browser* browser = web_app::LaunchSystemWebApp(
        profile, web_app::SystemAppType::SETTINGS, gurl, &did_create);
    ShowSettingsOnCurrentDesktop(browser);
    // Only notify if we created a new browser.
    if (!did_create || !browser)
      return;

    for (SettingsWindowManagerObserver& observer : observers_)
      observer.OnNewSettingsWindow(browser);

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
      return;
    }
  }
  if (browser) {
    NavigateParams params(browser, gurl, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.window_action = NavigateParams::SHOW_WINDOW;
    params.user_gesture = true;
    Navigate(&params);
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

  // operator[] not used because SessionID has no default constructor.
  settings_session_map_.emplace(profile, SessionID::InvalidValue())
      .first->second = browser->session_id();
  DCHECK(browser->is_trusted_source());

  auto* window = browser->window()->GetNativeWindow();
  window->SetProperty(kOverrideWindowIconResourceIdKey, IDR_SETTINGS_LOGO_192);
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::CHROME_APP));

  for (SettingsWindowManagerObserver& observer : observers_)
    observer.OnNewSettingsWindow(browser);
}

void SettingsWindowManager::ShowOSSettings(Profile* profile) {
  ShowOSSettings(profile, std::string());
}

void SettingsWindowManager::ShowOSSettings(Profile* profile,
                                           const std::string& sub_page) {
  ShowChromePageForProfile(profile, chrome::GetOSSettingsUrl(sub_page));
}

Browser* SettingsWindowManager::FindBrowserForProfile(Profile* profile) {
  if (web_app::SystemWebAppManager::IsEnabled()) {
    return web_app::FindSystemWebAppBrowser(profile,
                                            web_app::SystemAppType::SETTINGS);
  }

  auto iter = settings_session_map_.find(profile);
  if (iter != settings_session_map_.end())
    return chrome::FindBrowserWithID(iter->second);

  return nullptr;
}

bool SettingsWindowManager::IsSettingsBrowser(Browser* browser) const {
  Profile* profile = browser->profile();
  if (web_app::SystemWebAppManager::IsEnabled()) {
    // TODO(calamity): Determine whether, during startup, we need to wait for
    // app install and then provide a valid answer here.
    base::Optional<std::string> settings_app_id =
        web_app::GetAppIdForSystemWebApp(profile,
                                         web_app::SystemAppType::SETTINGS);
    return settings_app_id && browser->app_controller() &&
           browser->app_controller()->GetAppId() == settings_app_id.value();
  } else {
    auto iter = settings_session_map_.find(profile);
    return iter != settings_session_map_.end() &&
           iter->second == browser->session_id();
  }
}

SettingsWindowManager::SettingsWindowManager() {}

SettingsWindowManager::~SettingsWindowManager() {}

}  // namespace chrome
