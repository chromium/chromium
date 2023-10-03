// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"

#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace {

bool IsSaveableNavigation(content::NavigationHandle* navigation_handle) {
  ui::PageTransition page_transition = navigation_handle->GetPageTransition();
  if (navigation_handle->IsPost()) {
    return false;
  }
  if (!ui::IsValidPageTransitionType(page_transition)) {
    return false;
  }
  if (ui::PageTransitionIsRedirect(page_transition)) {
    return false;
  }
  if (!ui::PageTransitionIsMainFrame(page_transition)) {
    return false;
  }

  return SavedTabGroupUtils::IsURLValidForSavedTabGroups(
      navigation_handle->GetURL());
}

}  // namespace

SavedTabGroupWebContentsListener::SavedTabGroupWebContentsListener(
    content::WebContents* web_contents,
    base::Token token,
    SavedTabGroupModel* model)
    : token_(token), web_contents_(web_contents), model_(model) {
  Observe(web_contents_);
}

SavedTabGroupWebContentsListener::~SavedTabGroupWebContentsListener() = default;

void SavedTabGroupWebContentsListener::NavigateToUrl(const GURL& url) {
  // Dont navigate to the new URL if its not valid for sync.
  if (!SavedTabGroupUtils::IsURLValidForSavedTabGroups(url)) {
    return;
  }

  content::NavigationHandle* navigation_handle =
      web_contents()
          ->GetController()
          .LoadURLWithParams(content::NavigationController::LoadURLParams(url))
          .get();
  handle_from_sync_update_ = navigation_handle;
}

void SavedTabGroupWebContentsListener::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the navigation was the result of a sync update we don't want to update
  // the SavedTabGroupModel.
  if (navigation_handle == handle_from_sync_update_) {
    handle_from_sync_update_ = nullptr;
    return;
  }

  handle_from_sync_update_ = nullptr;

  if (!IsSaveableNavigation(navigation_handle)) {
    return;
  }

  SavedTabGroup* group = model_->GetGroupContainingTab(token_);
  CHECK(group);

  SavedTabGroupTab* tab = group->GetTab(token_);
  tab->SetTitle(web_contents_->GetTitle());
  tab->SetURL(web_contents_->GetURL());
  tab->SetFavicon(favicon::TabFaviconFromWebContents(web_contents_));
  model_->UpdateTabInGroup(group->saved_guid(), *tab);
}
