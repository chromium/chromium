// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/search_engine_choice/icon_utils.h"

#include <algorithm>

#include "base/check.h"

std::u16string GetGeneratedIconPath(
    const std::u16string& keyword,
    const std::u16string& parent_directory_path) {
  CHECK(parent_directory_path.back() == '/');

  std::u16string engine_keyword = keyword;
  std::replace(engine_keyword.begin(), engine_keyword.end(), '.', '_');
  std::replace(engine_keyword.begin(), engine_keyword.end(), '-', '_');

  return parent_directory_path + engine_keyword + u".png";
}
