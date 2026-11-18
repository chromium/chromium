// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_item_utils.h"

#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "url/gurl.h"

namespace offline_pages {

bool EqualsIgnoringFragment(const GURL& lhs, const GURL& rhs) {
  return UrlWithoutFragment(lhs) == UrlWithoutFragment(rhs);
}

GURL UrlWithoutFragment(const GURL& url) {
  GURL::Replacements remove_fragment;
  remove_fragment.ClearRef();
  return url.ReplaceComponents(remove_fragment);
}

OfflinePageUrlMatch ClassifyOfflinePageUrlMatch(const GURL& url,
                                                const OfflinePageItem& item) {
  if (!OfflinePageModel::CanSaveURL(url) ||
      !OfflinePageModel::CanSaveURL(item.url)) {
    return OfflinePageUrlMatch::kNone;
  }
  if (EqualsIgnoringFragment(url, item.url)) {
    return OfflinePageUrlMatch::kMatchesFinalUrl;
  }
  if (OfflinePageModel::CanSaveURL(item.original_url_if_different) &&
      EqualsIgnoringFragment(url, item.original_url_if_different)) {
    return OfflinePageUrlMatch::kMatchesOriginalUrl;
  }
  return OfflinePageUrlMatch::kNone;
}

}  // namespace offline_pages
