// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/string_util.h"

namespace app_list {

std::string NormalizeId(const std::string& id) {
  std::string result(id);
  // No existing scheme names include the delimiter string "://".
  std::size_t delimiter_index = result.find("://");
  if (delimiter_index != std::string::npos)
    result.erase(0, delimiter_index + 3);
  if (!result.empty() && result.back() == '/')
    result.pop_back();
  return result;
}

std::string RemoveAppShortcutLabel(const std::string& id) {
  std::string result(id);
  std::size_t delimiter_index = result.find_last_of('/');
  if (delimiter_index != std::string::npos)
    result.erase(delimiter_index);
  return result;
}

}  // namespace app_list
