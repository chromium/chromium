// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"

#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_observer.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/sad_tab.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

BookmarkTabHelper::~BookmarkTabHelper() {
  if (bookmark_model_) {
    bookmark_model_->RemoveObserver(this);
  }
}

void BookmarkTabHelper::AddObserver(BookmarkTabHelperObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkTabHelper::RemoveObserver(BookmarkTabHelperObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool BookmarkTabHelper::HasObserver(BookmarkTabHelperObserver* observer) const {
  return observers_.HasObserver(observer);
}

BookmarkTabHelper::BookmarkTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<BookmarkTabHelper>(*web_contents),
      is_starred_(false),
      bookmark_model_(nullptr),
      bookmark_drag_(nullptr) {
  bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());
  if (bookmark_model_) {
    bookmark_model_->AddObserver(this);
  }
}

void BookmarkTabHelper::UpdateStarredStateForCurrentURL() {
  const bool old_state = is_starred_;
  is_starred_ =
      (bookmark_model_ &&
       bookmark_model_->IsBookmarked(chrome::GetURLToBookmark(web_contents())));

  if (is_starred_ != old_state) {
    for (auto& observer : observers_) {
      observer.URLStarredChanged(web_contents(), is_starred_);
    }
  }
}

void BookmarkTabHelper::BookmarkModelChanged() {}

void BookmarkTabHelper::BookmarkModelLoaded(bool ids_reassigned) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkNodeAdded(const BookmarkNode* parent,
                                          size_t index,
                                          bool added_by_user) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkNodeRemoved(const BookmarkNode* parent,
                                            size_t old_index,
                                            const BookmarkNode* node,
                                            const std::set<GURL>& removed_urls,
                                            const base::Location& location) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkNodeChanged(const BookmarkNode* node) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  UpdateStarredStateForCurrentURL();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BookmarkTabHelper);
