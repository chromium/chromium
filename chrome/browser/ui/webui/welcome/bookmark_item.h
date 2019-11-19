// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_BOOKMARK_ITEM_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_BOOKMARK_ITEM_H_

#include <stddef.h>
#include <string>
#include <vector>

namespace base {
class ListValue;
}  // namespace base

namespace welcome {

struct BookmarkItem {
  const int id;
  const std::string name;
  const char* webui_icon;
  const std::string url;
  const int icon;  // Corresponds with resource ID, used for bookmark cache.
};

base::ListValue BookmarkItemsToListValue(const BookmarkItem items[],
                                         size_t count);

base::ListValue BookmarkItemsToListValue(
    const std::vector<BookmarkItem>& items);

}  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_BOOKMARK_ITEM_H_
