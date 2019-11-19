// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_visuals.h"

#include "components/offline_pages/core/offline_store_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

const base::Time kTestTime = store_utils::FromDatabaseTime(42);
const char kThumbnailData[] = "abc123";
const char kFaviconData[] = "favicondata";

TEST(OfflinePageVisualsTest, Construct) {
  OfflinePageVisuals visuals(1, kTestTime, kThumbnailData, kFaviconData);
  EXPECT_EQ(1, visuals.offline_id);
  EXPECT_EQ(kTestTime, visuals.expiration);
  EXPECT_EQ(kThumbnailData, visuals.thumbnail);
  EXPECT_EQ(kFaviconData, visuals.favicon);
}

TEST(OfflinePageVisualsTest, Equal) {
  OfflinePageVisuals visuals(1, kTestTime, kThumbnailData, kFaviconData);
  auto copy = visuals;

  EXPECT_EQ(1, copy.offline_id);
  EXPECT_EQ(kTestTime, copy.expiration);
  EXPECT_EQ(kThumbnailData, copy.thumbnail);
  EXPECT_EQ(kFaviconData, copy.favicon);
  EXPECT_EQ(copy, visuals);
}

TEST(OfflinePageVisualsTest, Compare) {
  OfflinePageVisuals visuals_a(1, kTestTime, kThumbnailData, kFaviconData);
  OfflinePageVisuals visuals_b(2, kTestTime, kThumbnailData, kFaviconData);

  EXPECT_TRUE(visuals_a < visuals_b);
  EXPECT_FALSE(visuals_b < visuals_a);
  EXPECT_FALSE(visuals_a < visuals_a);
}

TEST(OfflinePageVisualsTest, ToString) {
  OfflinePageVisuals visuals(1, kTestTime, kThumbnailData, kFaviconData);

  EXPECT_EQ(
      "OfflinePageVisuals(id=1, expiration=42, thumbnail=YWJjMTIz, "
      "favicon=ZmF2aWNvbmRhdGE=)",
      visuals.ToString());
}

}  // namespace
}  // namespace offline_pages
