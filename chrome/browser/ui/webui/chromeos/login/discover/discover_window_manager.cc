// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/window_properties.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_window_manager_observer.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace chromeos {

// static
DiscoverWindowManager* DiscoverWindowManager::GetInstance() {
  static base::NoDestructor<DiscoverWindowManager> window_manager;
  return window_manager.get();
}

void DiscoverWindowManager::AddObserver(
    DiscoverWindowManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void DiscoverWindowManager::RemoveObserver(
    const DiscoverWindowManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DiscoverWindowManager::ShowChromeDiscoverPageForProfile(Profile* profile) {
  const GURL gurl(chrome::kChromeUIDiscoverURL);

  // Use the original (non off-the-record) profile for discover unless
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
  // Adjust window size by the title bar size.
  // TODO(https://crbug.com/864686): remove this.
  params.window_bounds = gfx::Rect(768, 640 + 32 /* FIXMEalemate) */);

  Navigate(&params);

  // operator[] not used because SessionID has no default constructor.
  discover_session_map_.emplace(profile, SessionID::InvalidValue())
      .first->second = params.browser->session_id();
  DCHECK(params.browser->is_trusted_source());

  auto* window = params.browser->window()->GetNativeWindow();
  window->SetProperty(kOverrideWindowIconResourceIdKey, IDR_DISCOVER_APP_192);
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(ash::AppType::CHROME_APP));
  // Manually position the window in center of the screen.
  gfx::Rect center_in_screen =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  center_in_screen.ClampToCenteredSize(window->bounds().size());
  window->SetBounds(center_in_screen);

  for (DiscoverWindowManagerObserver& observer : observers_)
    observer.OnNewDiscoverWindow(params.browser);
}

Browser* DiscoverWindowManager::FindBrowserForProfile(Profile* profile) {
  ProfileSessionMap::iterator iter = discover_session_map_.find(profile);
  if (iter != discover_session_map_.end())
    return chrome::FindBrowserWithID(iter->second);
  return nullptr;
}

bool DiscoverWindowManager::IsDiscoverBrowser(Browser* browser) const {
  ProfileSessionMap::const_iterator iter =
      discover_session_map_.find(browser->profile());
  return (iter != discover_session_map_.end() &&
          iter->second == browser->session_id());
}

DiscoverWindowManager::DiscoverWindowManager() = default;

DiscoverWindowManager::~DiscoverWindowManager() = default;

}  // namespace chromeos
