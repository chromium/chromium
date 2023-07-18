// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waffle/waffle_tab_helper.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

WaffleTabHelper::~WaffleTabHelper() = default;

WaffleTabHelper::WaffleTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<WaffleTabHelper>(*web_contents) {
  CHECK(base::FeatureList::IsEnabled(switches::kWaffle));
}

void WaffleTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return;
  }

  // Only valid top frame and committed navigations are considered.
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Don't show the Waffle dialog on top of any sub page of the settings page.
  if (navigation_handle->GetURL().host() == chrome::kChromeUISettingsHost) {
    return;
  }

  if (auto* browser = chrome::FindBrowserWithWebContents(
          navigation_handle->GetWebContents())) {
    ShowWaffleDialog(*browser);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WaffleTabHelper);
