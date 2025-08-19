// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/boca/chrome_tab_strip_delegate.h"

#include "ash/public/cpp/tab_strip_delegate.h"
#include "chrome/browser/ash/browser_delegate/browser_controller.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

ChromeTabStripDelegate::ChromeTabStripDelegate() = default;
ChromeTabStripDelegate::~ChromeTabStripDelegate() = default;

std::vector<ash::TabInfo> ChromeTabStripDelegate::GetTabsListForWindow(
    aura::Window* window) const {
  if (!window) {
    return {};
  }

  // If the given `window` contains a browser frame
  ash::BrowserDelegate* browser =
      ash::BrowserController::GetInstance()->GetBrowserForWindow(window);

  // Not fetching incognito window.
  if (!browser || browser->IsOffTheRecord()) {
    return {};
  }

  std::vector<ash::TabInfo> tabs;
  for (size_t i = 0; i < browser->GetWebContentsCount(); i++) {
    ash::TabInfo tab;
    auto* web_contents = browser->GetWebContentsAt(i);
    tab.last_access_timetick = web_contents->GetLastActiveTimeTicks();
    // Not reading from web_contents->GetFaviconURLs() which is not reliable and
    // could be empty depends on the timing of the retrieval.
    content::NavigationEntry* entry =
        web_contents->GetController().GetLastCommittedEntry();
    tab.favicon = entry->GetFavicon().url;
    tab.title = web_contents->GetTitle();
    tab.url = web_contents->GetVisibleURL();
    tab.id = sessions::SessionTabHelper::IdForTab(web_contents).id();
    tabs.push_back(tab);
  }
  return tabs;
}
