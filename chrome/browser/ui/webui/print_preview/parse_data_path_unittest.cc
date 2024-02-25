// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/parse_data_path.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(ParseDataPathTest, ParseDataPath) {
  std::optional<PrintPreviewIdAndPageIndex> parsed =
      ParseDataPath("3/4/print.pdf");
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->ui_id, 3);
  EXPECT_EQ(parsed->page_index, 4);
}

TEST(ParseDataPathTest, ParseDataPathTest) {
  std::optional<PrintPreviewIdAndPageIndex> parsed =
      ParseDataPath("1/1/test.pdf");
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->ui_id, -1);
  EXPECT_EQ(parsed->page_index, 0);
}

TEST(ParseDataPathTest, ParseDataPathValid) {
  EXPECT_TRUE(ParseDataPath("1/2/print.pdf"));
}

TEST(ParseDataPathTest, ParseDataPathInvalid) {
  // Doesn't end in print.pdf
  EXPECT_FALSE(ParseDataPath("pdf/browser_api.js"));
  // Doesn't have both page index and UI ID.
  EXPECT_FALSE(ParseDataPath("1/print.pdf"));
  // Non-integer UI ID
  EXPECT_FALSE(ParseDataPath("foo/0/print.pdf"));
  // UI ID < 0
  EXPECT_FALSE(ParseDataPath("-1/0/print.pdf"));
  // Non-integer page index
  EXPECT_FALSE(ParseDataPath("1/foo/print.pdf"));
}

}  // namespace printing
