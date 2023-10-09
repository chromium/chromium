// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/focus_tab_after_navigation_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

FocusTabAfterNavigationHelper::FocusTabAfterNavigationHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<FocusTabAfterNavigationHelper>(*contents) {}

FocusTabAfterNavigationHelper::~FocusTabAfterNavigationHelper() = default;

void FocusTabAfterNavigationHelper::ReadyToCommitNavigation(
    content::NavigationHandle* navigation) {
  // Focus the tab contents if needed.  This is done at the ReadyToCommit time
  // to:
  // 1) ignore same-document navigations (ReadyToCommitNavigation method is not
  //    invoked for same-document navigations)
  // 2) postpone moving the focus until we are ready to commit the page
  // 3) move the focus before the page starts rendering
  // (only 1 is a hard-requirement;  2 and 3 seem desirable but there are no
  // known scenarios where violating these requirements would lead to bugs).
  if (ShouldFocusTabContents(navigation))
    web_contents()->SetInitialFocus();
}

bool FocusTabAfterNavigationHelper::ShouldFocusTabContents(
    content::NavigationHandle* navigation) {
  // Don't focus content in an inactive window or tab.
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser)
    return false;
  if (!browser->window()->IsActive())
    return false;
  if (browser->tab_strip_model()->GetActiveWebContents() != web_contents())
    return false;

  // Don't focus content after subframe navigations.
  if (!navigation->IsInPrimaryMainFrame())
    return false;

  // Browser-initiated navigations (e.g. typing in an omnibox) are taken care of
  // in Browser::UpdateUIForNavigationInTab.  See also https://crbug.com/1048591
  // for possible regression risks related to returning |true| here.
  if (!navigation->IsRendererInitiated())
    return false;

  // Renderer-initiated navigations shouldn't focus the tab contents, unless the
  // navigation is leaving the NTP.  See also https://crbug.com/1027719.
  bool started_at_ntp = IsNtpURL(web_contents()->GetLastCommittedURL());
  if (!started_at_ntp)
    return false;

  // Navigations initiated via chrome.tabs.update and similar APIs should not
  // steal focus from the omnibox.  See also https://crbug.com/1085779.
  if (navigation->GetPageTransition() & ui::PAGE_TRANSITION_FROM_API)
    return false;

  // Rewrite chrome://newtab to compare with the navigation URL.
  GURL rewritten_ntp_url = web_contents()->GetLastCommittedURL();
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &rewritten_ntp_url, profile);

  // Focus if the destination is not the NTP.
  return !IsNtpURL(navigation->GetURL()) &&
         (navigation->GetURL() != rewritten_ntp_url);
}

bool FocusTabAfterNavigationHelper::IsNtpURL(const GURL& url) {
  // TODO(lukasza): https://crbug.com/1034999: Try to avoid special-casing
  // kChromeUINewTabURL below and covering it via IsNTPOrRelatedURL instead.
  if (url == GURL(chrome::kChromeUINewTabURL))
    return true;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return search::IsNTPOrRelatedURL(url, profile);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FocusTabAfterNavigationHelper);
