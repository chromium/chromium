// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/data_request_filter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace printing {

TEST(DataRequestFilterTest, ParseDataPath) {
  absl::optional<PrintPreviewIdAndPageIndex> parsed =
      ParseDataPath("3/4/print.pdf");
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->ui_id, 3);
  EXPECT_EQ(parsed->page_index, 4);
}

TEST(DataRequestFilterTest, ParseDataPathValid) {
  EXPECT_TRUE(ParseDataPath("1/2/print.pdf"));
}

TEST(DataRequestFilterTest, ParseDataPathInvalid) {
  EXPECT_FALSE(ParseDataPath("pdf/browser_api.js"));
}

}  // namespace printing
