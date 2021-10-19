// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_url_window_manager.h"

#include "ash/constants/app_types.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/chrome_url_window_manager_observer.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/aura_constants.h"
#include "url/gurl.h"

ChromeUrlWindowManager::ChromeUrlWindowManager() {
  BrowserList::AddObserver(this);
}

ChromeUrlWindowManager::~ChromeUrlWindowManager() {
  BrowserList::RemoveObserver(this);
}

void ChromeUrlWindowManager::AddObserver(
    ChromeUrlWindowManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ChromeUrlWindowManager::RemoveObserver(
    ChromeUrlWindowManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ChromeUrlWindowManager::ShowChromePageForProfile(Profile* profile,
                                                      const GURL& gurl,
                                                      int64_t display_id) {
  // Use the original (non off-the-record) profile for a Chrome URL unless
  // this is a guest session.
  if (!profile->IsGuestSession() && profile->IsOffTheRecord())
    profile = profile->GetOriginalProfile();

  // If this profile isn't allowed to create browser windows (e.g. the login
  // screen profile) then bail out.
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return;
  }

  if (profile != nullptr) {  // Temporary: "if (true)" - will removed rest next.
    web_app::SystemAppLaunchParams params;
    params.url = gurl;
    web_app::LaunchSystemWebAppAsync(profile,
                                     web_app::SystemAppType::OS_URL_HANDLER,
                                     params, apps::MakeWindowInfo(display_id));
    return;
  }
  // TODO(skuhne): Remove below and the other associated files next!

  // Look for an existing Chrome url browser window.
  Browser* browser = FindBrowserForProfileAndUrl(profile, gurl);
  if (browser) {
    DCHECK(browser->profile() == profile);
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetWebContentsAt(0);
    if (web_contents && web_contents->GetURL() == gurl) {
      browser->window()->Show();
      return;
    }

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
  CHECK(browser);

  // operator[] not used because SessionID has no default constructor.
  chrome_url_session_map_.emplace(profile, SessionID::InvalidValue())
      .first->second = browser->session_id();
  DCHECK(browser->is_trusted_source());

  auto* window = browser->window()->GetNativeWindow();
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::CHROME_APP));
  // TODO(crbug/1256494): Find the URL dependent app logo.
  window->SetProperty(kOverrideWindowIconResourceIdKey, IDR_SETTINGS_LOGO_192);

  for (ChromeUrlWindowManagerObserver& observer : observers_)
    observer.OnNewChromeUrlWindow(browser);
}

void ChromeUrlWindowManager::OnBrowserRemoved(Browser* browser) {
  for (auto it = chrome_url_session_map_.begin();
       it != chrome_url_session_map_.end(); ++it) {
    // TODO(crbug/1256497): Once we go to more URLs, this need to be changed.
    if (chrome::FindBrowserWithID(it->second) == browser) {
      chrome_url_session_map_.erase(it);
      return;
    }
  }
}

Browser* ChromeUrlWindowManager::FindBrowserForProfileAndUrl(Profile* profile,
                                                             const GURL& gurl) {
  // TODO(crbug/1256497): Instead of having a single app window, allow for
  // multiple app windows (per url). Also - Ash should only know one user
  // profile.
  auto iter = chrome_url_session_map_.find(profile);
  return (iter != chrome_url_session_map_.end())
             ? chrome::FindBrowserWithID(iter->second)
             : nullptr;
}
