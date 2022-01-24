// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/data_request_filter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(DataRequestFilterTest, ParseDataPath) {
  int ui_id = -1;
  int page_index = -2;
  EXPECT_TRUE(ParseDataPath("3/4/print.pdf", &ui_id, &page_index));

  EXPECT_EQ(ui_id, 3);
  EXPECT_EQ(page_index, 4);
}

TEST(DataRequestFilterTest, ParseDataPathValid) {
  EXPECT_TRUE(ParseDataPath("1/2/print.pdf", nullptr, nullptr));
}

TEST(DataRequestFilterTest, ParseDataPathInvalid) {
  EXPECT_FALSE(ParseDataPath("pdf/browser_api.js", nullptr, nullptr));
}

}  // namespace printing
