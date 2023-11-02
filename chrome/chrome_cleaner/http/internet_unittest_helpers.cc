// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/http/internet_unittest_helpers.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

void ExpectMultipartMimeMessageIsPlausible(
    const std::wstring& boundary,
    const std::map<std::wstring, std::wstring>& parameters,
    const std::string& file,
    const std::string& file_part_name,
    const std::string& body) {
  std::string::const_iterator range_begin = body.begin();
  if (!parameters.empty()) {
    std::string key = base::WideToUTF8(parameters.begin()->first);
    std::string value = base::WideToUTF8(parameters.begin()->second);
    range_begin = std::search(range_begin, body.end(), key.begin(), key.end());
    EXPECT_NE(range_begin, body.end());
    range_begin =
        std::search(range_begin, body.end(), value.begin(), value.end());
    EXPECT_NE(range_begin, body.end());
  }

  range_begin =
      std::search(range_begin, body.end(), boundary.begin(), boundary.end());
  EXPECT_NE(range_begin, body.end());
  range_begin = std::search(range_begin, body.end(), file_part_name.begin(),
                            file_part_name.end());
  EXPECT_NE(range_begin, body.end());
  range_begin = std::search(range_begin, body.end(), file.begin(), file.end());
  EXPECT_NE(range_begin, body.end());
}

}  // namespace chrome_cleaner
