// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_BOOKMARK_ITEM_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_BOOKMARK_ITEM_H_

#include <stddef.h>

namespace base {
class ListValue;
}  // namespace base

namespace nux {

struct BookmarkItem {
  const int id;
  const char* name;
  const char* webui_icon;
  const char* url;
  const int icon;  // Corresponds with resource ID, used for bookmark cache.
};

base::ListValue bookmarkItemsToListValue(const BookmarkItem items[],
                                         size_t count);

}  // namespace nux

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_BOOKMARK_ITEM_H_
