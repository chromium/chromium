// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_UTILS_H_

class GURL;

namespace offline_pages {

// Returns true if two URLs are equal, ignoring the fragment.
// Because offline page items are stored without fragment, this is appropriate
// for checking if an offline item's URL matches another URL.
bool EqualsIgnoringFragment(const GURL& lhs, const GURL& rhs);

// Strips any fragment from |url| and returns the result.
GURL UrlWithoutFragment(const GURL& url);

}  // namespace offline_pages
#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ITEM_UTILS_H_
