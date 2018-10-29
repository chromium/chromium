// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/bookmark_item.h"

#include "base/strings/string16.h"
#include "base/values.h"

namespace nux {

base::ListValue bookmarkItemsToListValue(const BookmarkItem items[],
                                         size_t count) {
  base::ListValue list_value;
  for (size_t i = 0; i < count; ++i) {
    std::unique_ptr<base::DictionaryValue> element =
        std::make_unique<base::DictionaryValue>();

    element->SetInteger("id", static_cast<int>(items[i].id));
    element->SetString("name", items[i].name);
    element->SetString("icon", items[i].webui_icon);
    element->SetString("url", items[i].url);

    list_value.Append(std::move(element));
  }
  return list_value;
}

}  // namespace nux
