// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/bookmark_item.h"

#include "base/strings/string16.h"
#include "base/values.h"

namespace welcome {

base::ListValue BookmarkItemsToListValue(const BookmarkItem items[],
                                         size_t count) {
  base::ListValue list_value;
  for (size_t i = 0; i < count; ++i) {
    auto element = std::make_unique<base::DictionaryValue>();

    element->SetInteger("id", items[i].id);
    element->SetString("name", items[i].name);
    element->SetString("icon", items[i].webui_icon);
    element->SetString("url", items[i].url);

    list_value.Append(std::move(element));
  }
  return list_value;
}

base::ListValue BookmarkItemsToListValue(
    const std::vector<BookmarkItem>& items) {
  base::ListValue list_value;
  for (const auto& item : items) {
    auto element = std::make_unique<base::DictionaryValue>();

    element->SetInteger("id", item.id);
    element->SetString("name", item.name);
    element->SetString("icon", item.webui_icon);
    element->SetString("url", item.url);

    list_value.Append(std::move(element));
  }
  return list_value;
}

}  // namespace welcome
