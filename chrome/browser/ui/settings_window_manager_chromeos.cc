// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/settings_window_manager_chromeos.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_observer_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/aura_constants.h"
#include "url/gurl.h"

namespace chrome {

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

  // Look for an existing browser window.
  Browser* browser = FindBrowserForProfile(profile);
  if (browser) {
    DCHECK(browser->profile() == profile);
    const content::WebContents* web_contents =
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

  // operator[] not used because SessionID has no default constructor.
  settings_session_map_.emplace(profile, SessionID::InvalidValue())
      .first->second = params.browser->session_id();
  DCHECK(params.browser->is_trusted_source());

  auto* window = params.browser->window()->GetNativeWindow();
  window->SetProperty(kOverrideWindowIconResourceIdKey, IDR_SETTINGS_LOGO_192);
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::CHROME_APP));

  for (SettingsWindowManagerObserver& observer : observers_)
    observer.OnNewSettingsWindow(params.browser);
}

Browser* SettingsWindowManager::FindBrowserForProfile(Profile* profile) {
  ProfileSessionMap::iterator iter = settings_session_map_.find(profile);
  if (iter != settings_session_map_.end())
    return chrome::FindBrowserWithID(iter->second);
  return NULL;
}

bool SettingsWindowManager::IsSettingsBrowser(Browser* browser) const {
  ProfileSessionMap::const_iterator iter =
      settings_session_map_.find(browser->profile());
  return (iter != settings_session_map_.end() &&
          iter->second == browser->session_id());
}

SettingsWindowManager::SettingsWindowManager() {}

SettingsWindowManager::~SettingsWindowManager() {}

}  // namespace chrome
