// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_TEST_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_TEST_UTILS_H_

#include <string>

class GURL;
class PrefService;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace commerce {

// Create a product bookmark with the specified cluster ID and place it in the
// "other" bookmarks folder.
const bookmarks::BookmarkNode* AddProductBookmark(
    bookmarks::BookmarkModel* bookmark_model,
    const std::u16string& title,
    const GURL& url,
    uint64_t cluster_id,
    bool is_price_tracked = false,
    const int64_t price_micros = 0L,
    const std::string& currency_code = "usd");

// Sets the state of the enterprise policy for the shopping list feature for
// testing.
void SetShoppingListEnterprisePolicyPref(PrefService* prefs, bool enabled);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_TEST_UTILS_H_
