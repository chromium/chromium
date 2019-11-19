// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_item_utils.h"
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

}  // namespace offline_pages
