// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_client.h"

#include "base/notreached.h"

namespace bookmarks {

void BookmarkClient::Init(BookmarkModel* model) {}

void BookmarkClient::RequiredRecoveryToLoad(
    const std::multimap<int64_t, int64_t>&
        local_or_syncable_reassigned_ids_per_old_id) {}

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
  NOTREACHED_IN_MIGRATION();
}

}  // namespace bookmarks
