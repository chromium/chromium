// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_client.h"

#include "base/logging.h"

namespace bookmarks {

void BookmarkClient::Init(BookmarkModel* model) {}

bool BookmarkClient::PreferTouchIcon() {
  return false;
}

base::CancelableTaskTracker::TaskId BookmarkClient::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::IconType type,
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

}  // namespace bookmarks
