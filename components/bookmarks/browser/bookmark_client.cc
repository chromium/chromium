// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_client.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "components/bookmarks/common/bookmark_features.h"

namespace bookmarks {

void BookmarkClient::Init(BookmarkModel* model) {}

const BookmarkNode* BookmarkClient::GetSuggestedSaveLocation(const GURL& url) {
  return nullptr;
}

base::CancelableTaskTracker::TaskId BookmarkClient::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    base::CancelableTaskTracker* tracker) {
  return base::CancelableTaskTracker::kBadTaskId;
}

bool BookmarkClient::SupportsTypedCountForUrls() {
  return false;
}

void BookmarkClient::GetTypedCountForUrls(
    UrlTypedCountMap* url_typed_count_map) {
  NOTREACHED();
}

bool BookmarkClient::IsPermanentNodeVisibleWhenEmpty(
    bookmarks::BookmarkNode::Type type) const {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  bool is_desktop = false;
#else
  bool is_desktop = true;
#endif

  switch (type) {
    case BookmarkNode::URL:
      NOTREACHED_NORETURN();
    case BookmarkNode::FOLDER:
      // Managed node.
      return false;
    case BookmarkNode::BOOKMARK_BAR:
      return is_desktop;
    case BookmarkNode::OTHER_NODE:
      return is_desktop || base::FeatureList::IsEnabled(
                               kAllBookmarksBaselineFolderVisibility);
    case BookmarkNode::MOBILE:
      // Either MOBILE or OTHER_NODE is visible when empty, but never both.
      return !IsPermanentNodeVisibleWhenEmpty(BookmarkNode::OTHER_NODE);
  }
  NOTREACHED_NORETURN();
}

}  // namespace bookmarks
