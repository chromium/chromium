// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"

namespace app_list {

std::u16string RemoveDebugPrefix(const std::u16string str) {
  std::string result = base::UTF16ToUTF8(str);

  if (result.empty() || result[0] != '(')
    return str;

  const std::size_t delimiter_index = result.find(") ");
  if (delimiter_index != std::string::npos)
    result.erase(0, delimiter_index + 2);
  return base::UTF8ToUTF16(result);
}

std::u16string RemoveTopMatchPrefix(const std::u16string str) {
  const std::string top_match_details = kTopMatchDetails;
  std::string result = base::UTF16ToUTF8(str);

  if (result.empty() || result.rfind(top_match_details, 0u) != 0)
    return str;

  result.erase(0, top_match_details.size());
  return base::UTF8ToUTF16(result);
}

}  // namespace app_list
