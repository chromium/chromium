// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/bookmark_item.h"

#include <string>

#include "base/containers/span.h"
#include "base/values.h"

namespace welcome {

base::Value::List BookmarkItemsToListValue(
    base::span<const BookmarkItem> items) {
  base::Value::List list_value;
  for (const auto& item : items) {
    base::Value::Dict element;

    element.Set("id", item.id);
    element.Set("name", item.name);
    element.Set("icon", item.webui_icon);
    element.Set("url", item.url);

    list_value.Append(std::move(element));
  }
  return list_value;
}

}  // namespace welcome
