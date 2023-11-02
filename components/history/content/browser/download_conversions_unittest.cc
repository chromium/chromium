// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/download_conversions.h"

#include <vector>

#include "components/history/core/browser/download_slice_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

TEST(DownloadConversionsTest, ToContentReceivedSlices) {
  std::vector<DownloadSliceInfo> info;
  int64_t offset1 = 0;
  int64_t received1 = 100;
  int64_t offset2 = 1000;
  int64_t received2 = 50;
  DownloadId id = 1;
  info.emplace_back(id, offset1, received1, false);
  info.emplace_back(id, offset2, received2, true);
  std::vector<download::DownloadItem::ReceivedSlice> received_slices =
      ToContentReceivedSlices(info);
  EXPECT_EQ(2u, received_slices.size());

  EXPECT_EQ(offset1, received_slices[0].offset);
  EXPECT_EQ(received1, received_slices[0].received_bytes);
  EXPECT_FALSE(received_slices[0].finished);

  EXPECT_EQ(offset2, received_slices[1].offset);
  EXPECT_EQ(received2, received_slices[1].received_bytes);
  EXPECT_TRUE(received_slices[1].finished);
}

}  // namespace history
