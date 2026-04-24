// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/singleton_tabs.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_navigator_params_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"

void ShowSingletonTab(Profile* profile, const GURL& url) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  NavigateParams params(
      GetSingletonTabNavigateParams(displayer.browser(), url));
  Navigate(&params);
}

void ShowSingletonTab(BrowserWindowInterface* browser, const GURL& url) {
  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  Navigate(&params);
}

void ShowSingletonTabOverwritingNTP(
    Profile* profile,
    const GURL& url,
    NavigateParams::PathBehavior path_behavior) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  NavigateParams params(
      GetSingletonTabNavigateParams(displayer.browser(), url));
  params.path_behavior = path_behavior;
  ShowSingletonTabOverwritingNTP(&params);
}

void ShowSingletonTabOverwritingNTP(
    BrowserWindowInterface* browser,
    const GURL& url,
    NavigateParams::PathBehavior path_behavior) {
  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  params.path_behavior = path_behavior;
  ShowSingletonTabOverwritingNTP(&params);
}

void ShowSingletonTabOverwritingNTP(NavigateParams* params) {
  DCHECK_EQ(params->disposition, WindowOpenDisposition::SINGLETON_TAB);
  content::WebContents* contents =
      params->browser->GetTabStripModel()->GetActiveWebContents();
  if (contents) {
    const GURL& contents_url = contents->GetVisibleURL();
    if (contents_url == chrome::ChromeUINewTabURLAsGURL() ||
        search::IsInstantNTP(contents) || contents_url == url::kAboutBlankURL) {
      int tab_index =
          GetIndexOfExistingTabMatchingURL(params->browser, *params);
      if (tab_index < 0) {
        params->disposition = WindowOpenDisposition::CURRENT_TAB;
      } else {
        params->switch_to_singleton_tab =
            params->browser->GetTabStripModel()->GetWebContentsAt(tab_index);
      }
    }
  }
  Navigate(params);
}

NavigateParams GetSingletonTabNavigateParams(BrowserWindowInterface* browser,
                                             const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.user_gesture = true;
  params.tabstrip_add_types |= AddTabTypes::ADD_INHERIT_OPENER;
  return params;
}

