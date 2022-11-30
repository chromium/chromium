// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/download_slice_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace history {

TEST(DownloadSliceInfoTest, CompareDownloadSliceInfo) {
  DownloadSliceInfo info1(1, 500, 10, false);
  DownloadSliceInfo info2(1, 500, 1000, true);
  EXPECT_FALSE(info1 == info2);

  info1.received_bytes = 1000;
  EXPECT_FALSE(info1 == info2);

  info1.finished = true;
  EXPECT_TRUE(info1 == info2);

  info1.offset = 1;
  EXPECT_FALSE(info1 == info2);
}

}  // namespace history
