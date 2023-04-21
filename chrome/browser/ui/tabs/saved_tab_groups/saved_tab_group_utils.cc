// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"

#include "base/uuid.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents.h"

SavedTabGroupTab SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
    content::WebContents* contents,
    base::Uuid saved_tab_group_id) {
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

// static
Browser* SavedTabGroupUtils::GetBrowserWithTabGroupId(
    tab_groups::TabGroupId group_id) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model()->group_model()->ContainsTabGroup(group_id)) {
      return browser;
    }
  }
  return nullptr;
}

// static
TabGroup* SavedTabGroupUtils::GetTabGroupWithId(
    tab_groups::TabGroupId group_id) {
  Browser* browser = GetBrowserWithTabGroupId(group_id);
  if (!browser || !browser->tab_strip_model() ||
      !browser->tab_strip_model()->SupportsTabGroups()) {
    return nullptr;
  }

  TabGroupModel* tab_group_model = browser->tab_strip_model()->group_model();
  CHECK(tab_group_model);

  return tab_group_model->GetTabGroup(group_id);
}

// static
std::vector<content::WebContents*> SavedTabGroupUtils::GetWebContentsesInGroup(
    tab_groups::TabGroupId group_id) {
  Browser* browser = GetBrowserWithTabGroupId(group_id);
  if (!browser || !browser->tab_strip_model() ||
      !browser->tab_strip_model()->SupportsTabGroups()) {
    return {};
  }

  const gfx::Range local_tab_group_indices =
      SavedTabGroupUtils::GetTabGroupWithId(group_id)->ListTabs();
  std::vector<content::WebContents*> contentses;
  for (size_t index = local_tab_group_indices.start();
       index < local_tab_group_indices.end(); index++) {
    contentses.push_back(browser->tab_strip_model()->GetWebContentsAt(index));
  }
  return contentses;
}
