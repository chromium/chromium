// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/singleton_tabs.h"

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/web_contents.h"

namespace {

// Returns true if two URLs are equal after taking |replacements| into account.
bool CompareURLsWithReplacements(const GURL& url,
                                 const GURL& other,
                                 const GURL::Replacements& replacements,
                                 ChromeAutocompleteProviderClient* client) {
  GURL url_replaced = url.ReplaceComponents(replacements);
  GURL other_replaced = other.ReplaceComponents(replacements);
  return client->StrippedURLsAreEqual(url_replaced, other_replaced, nullptr);
}

}  // namespace

void ShowSingletonTab(Profile* profile, const GURL& url) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  NavigateParams params(
      GetSingletonTabNavigateParams(displayer.browser(), url));
  Navigate(&params);
}

void ShowSingletonTab(Browser* browser, const GURL& url) {
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
    Browser* browser,
    const GURL& url,
    NavigateParams::PathBehavior path_behavior) {
  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  params.path_behavior = path_behavior;
  ShowSingletonTabOverwritingNTP(&params);
}

void ShowSingletonTabOverwritingNTP(NavigateParams* params) {
  DCHECK_EQ(params->disposition, WindowOpenDisposition::SINGLETON_TAB);
  content::WebContents* contents =
      params->browser->tab_strip_model()->GetActiveWebContents();
  if (contents) {
    const GURL& contents_url = contents->GetVisibleURL();
    if (contents_url == chrome::kChromeUINewTabURL ||
        search::IsInstantNTP(contents) || contents_url == url::kAboutBlankURL) {
      int tab_index = GetIndexOfExistingTab(params->browser, *params);
      if (tab_index < 0) {
        params->disposition = WindowOpenDisposition::CURRENT_TAB;
      } else {
        params->switch_to_singleton_tab =
            params->browser->tab_strip_model()->GetWebContentsAt(tab_index);
      }
    }
  }
  Navigate(params);
}

NavigateParams GetSingletonTabNavigateParams(Browser* browser,
                                             const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  params.tabstrip_add_types |= AddTabTypes::ADD_INHERIT_OPENER;
  return params;
}

// Returns the index of an existing singleton tab in |browser| matching
// the URL specified in |params|.
int GetIndexOfExistingTab(Browser* browser, const NavigateParams& params) {
  if (params.disposition != WindowOpenDisposition::SINGLETON_TAB &&
      params.disposition != WindowOpenDisposition::SWITCH_TO_TAB)
    return -1;

  // In case the URL was rewritten by the BrowserURLHandler we need to ensure
  // that we do not open another URL that will get redirected to the rewritten
  // URL.
  const bool target_is_view_source =
      params.url.SchemeIs(content::kViewSourceScheme);
  GURL rewritten_url(params.url);
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &rewritten_url, browser->profile());

  ChromeAutocompleteProviderClient client(browser->profile());
  // If there are several matches: prefer the active tab by starting there.
  int start_index = std::max(0, browser->tab_strip_model()->active_index());
  int tab_count = browser->tab_strip_model()->count();
  for (int i = 0; i < tab_count; ++i) {
    int tab_index = (start_index + i) % tab_count;
    content::WebContents* tab =
        browser->tab_strip_model()->GetWebContentsAt(tab_index);

    GURL tab_url = tab->GetVisibleURL();

    // RewriteURLIfNecessary removes the "view-source:" scheme which could lead
    // to incorrect matching, so ensure that the target and the candidate are
    // either both view-source:, or neither is.
    if (tab_url.SchemeIs(content::kViewSourceScheme) != target_is_view_source) {
      continue;
    }

    GURL rewritten_tab_url = tab_url;
    content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
        &rewritten_tab_url, browser->profile());

    GURL::Replacements replacements;
    replacements.ClearRef();
    if (params.path_behavior == NavigateParams::IGNORE_AND_NAVIGATE) {
      replacements.ClearPath();
      replacements.ClearQuery();
    }

    if (CompareURLsWithReplacements(tab_url, params.url, replacements,
                                    &client) ||
        CompareURLsWithReplacements(rewritten_tab_url, rewritten_url,
                                    replacements, &client)) {
      return tab_index;
    }
  }

  return -1;
}

std::pair<Browser*, int> GetIndexAndBrowserOfExistingTab(
    Profile* profile,
    const NavigateParams& params) {
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    // When tab switching, only look at same profile and anonymity level.
    if (profile == browser->profile() && !browser->is_delete_scheduled()) {
      int index = GetIndexOfExistingTab(browser, params);
      if (index >= 0)
        return {browser, index};
    }
  }
  return {nullptr, -1};
}
