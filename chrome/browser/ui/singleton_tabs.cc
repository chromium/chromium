// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/singleton_tabs.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/web_contents.h"
namespace {

// Returns true if two URLs are equal after taking |replacements| into account.
bool CompareURLsWithReplacements(const GURL& url,
                                 const GURL& other,
                                 const GURL::Replacements& replacements,
                                 TemplateURLService* template_url_service) {
  GURL url_replaced = url.ReplaceComponents(replacements);
  GURL other_replaced = other.ReplaceComponents(replacements);
  AutocompleteInput input;
  return AutocompleteMatch::GURLToStrippedGURL(
             url_replaced, input, template_url_service, std::u16string(),
             /*keep_search_intent_params=*/false) ==
         AutocompleteMatch::GURLToStrippedGURL(
             other_replaced, input, template_url_service, std::u16string(),
             /*keep_search_intent_params=*/false);
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
  content::WebContents* contents = params->browser->GetBrowserForMigrationOnly()
                                       ->tab_strip_model()
                                       ->GetActiveWebContents();
  if (contents) {
    const GURL& contents_url = contents->GetVisibleURL();
    if (contents_url == chrome::kChromeUINewTabURL ||
        search::IsInstantNTP(contents) || contents_url == url::kAboutBlankURL) {
      int tab_index = GetIndexOfExistingTab(params->browser, *params);
      if (tab_index < 0) {
        params->disposition = WindowOpenDisposition::CURRENT_TAB;
      } else {
        params->switch_to_singleton_tab =
            params->browser->GetBrowserForMigrationOnly()
                ->tab_strip_model()
                ->GetWebContentsAt(tab_index);
      }
    }
  }
  Navigate(params);
}

NavigateParams GetSingletonTabNavigateParams(Browser* browser,
                                             const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  params.user_gesture = true;
  params.tabstrip_add_types |= AddTabTypes::ADD_INHERIT_OPENER;
  return params;
}

// Returns the index of an existing singleton tab in |browser| matching
// the URL specified in |params|.
int GetIndexOfExistingTab(BrowserWindowInterface* browser,
                          const NavigateParams& params) {
  if (params.disposition != WindowOpenDisposition::SINGLETON_TAB &&
      params.disposition != WindowOpenDisposition::SWITCH_TO_TAB) {
    return -1;
  }

  // In case the URL was rewritten by the BrowserURLHandler we need to ensure
  // that we do not open another URL that will get redirected to the rewritten
  // URL.
  const bool target_is_view_source =
      params.url.SchemeIs(content::kViewSourceScheme);
  GURL rewritten_url(params.url);
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &rewritten_url, browser->GetProfile());

  TemplateURLService* turl_service =
      TemplateURLServiceFactory::GetForProfile(browser->GetProfile());
  // If there are several matches: prefer the active tab by starting there.
  int start_index =
      std::max(0, browser->GetFeatures().tab_strip_model()->active_index());
  int tab_count = browser->GetFeatures().tab_strip_model()->count();
  for (int i = 0; i < tab_count; ++i) {
    int tab_index = (start_index + i) % tab_count;
    content::WebContents* tab =
        browser->GetFeatures().tab_strip_model()->GetWebContentsAt(tab_index);

    GURL tab_url = tab->GetVisibleURL();

    // RewriteURLIfNecessary removes the "view-source:" scheme which could lead
    // to incorrect matching, so ensure that the target and the candidate are
    // either both view-source:, or neither is.
    if (tab_url.SchemeIs(content::kViewSourceScheme) != target_is_view_source) {
      continue;
    }

    GURL rewritten_tab_url = tab_url;
    content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
        &rewritten_tab_url, browser->GetProfile());

    GURL::Replacements replacements;
    replacements.ClearRef();
    if (params.path_behavior == NavigateParams::IGNORE_AND_NAVIGATE) {
      replacements.ClearPath();
      replacements.ClearQuery();
    }

    if (CompareURLsWithReplacements(tab_url, params.url, replacements,
                                    turl_service) ||
        CompareURLsWithReplacements(rewritten_tab_url, rewritten_url,
                                    replacements, turl_service)) {
      return tab_index;
    }
  }

  return -1;
}

std::pair<BrowserWindowInterface*, int> GetIndexAndBrowserOfExistingTab(
    Profile* profile,
    const NavigateParams& params) {
  BrowserWindowInterface* browser_of_existing_tab = nullptr;
  int idx = -1;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        // When tab switching, only look at same profile and anonymity level.
        if (profile == browser->GetProfile() &&
            !browser->GetBrowserForMigrationOnly()->is_delete_scheduled()) {
          int index = GetIndexOfExistingTab(browser, params);
          if (index >= 0) {
            browser_of_existing_tab = browser;
            idx = index;
            return false;  // stop iterating
          }
        }
        return true;  // continue iterating
      });
  return {browser_of_existing_tab, idx};
}
