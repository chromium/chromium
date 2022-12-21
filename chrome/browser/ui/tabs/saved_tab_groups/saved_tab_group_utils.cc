// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "base/guid.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "content/public/browser/web_contents.h"

SavedTabGroupTab SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
    content::WebContents* contents,
    base::GUID saved_tab_group_id) {
  SavedTabGroupTab tab(contents->GetVisibleURL(), contents->GetTitle(),
                       saved_tab_group_id);
  tab.SetFavicon(favicon::TabFaviconFromWebContents(contents));
  return tab;
}

content::WebContents* SavedTabGroupUtils::OpenTabInBrowser(
    const GURL& url,
    Browser* browser,
    Profile* profile,
    WindowOpenDisposition disposition) {
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = disposition;
  params.browser = browser;
  base::WeakPtr<content::NavigationHandle> handle = Navigate(&params);
  return handle ? handle->GetWebContents() : nullptr;
}
