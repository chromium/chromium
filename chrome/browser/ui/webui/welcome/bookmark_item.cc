// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/bookmark_item.h"

#include <string>

#include "base/values.h"

namespace welcome {

base::ListValue BookmarkItemsToListValue(const BookmarkItem items[],
                                         size_t count) {
  base::ListValue list_value;
  for (size_t i = 0; i < count; ++i) {
    base::Value::Dict element;

    element.Set("id", items[i].id);
    element.Set("name", items[i].name);
    element.Set("icon", items[i].webui_icon);
    element.Set("url", items[i].url);

    list_value.Append(base::Value(std::move(element)));
  }
  return list_value;
}

}  // namespace welcome
