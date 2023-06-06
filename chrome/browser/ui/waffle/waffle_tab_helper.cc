// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waffle/waffle_tab_helper.h"

#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

WaffleTabHelper::~WaffleTabHelper() = default;

WaffleTabHelper::WaffleTabHelper(content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<WaffleTabHelper>(*web_contents) {
  CHECK(base::FeatureList::IsEnabled(kWaffle));
}

void WaffleTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Only valid top frame navigations are considered.
  if (!navigation_handle || !navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (auto* browser = chrome::FindBrowserWithWebContents(
          navigation_handle->GetWebContents())) {
    ShowWaffleDialog(*browser);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WaffleTabHelper);
