// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/feed_service.h"
#include "components/history/core/browser/history_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace feed {
namespace internal {
namespace {
using history::DeletionInfo;

TEST(ShouldClearFeed, ShouldClearFeed) {
  EXPECT_TRUE(ShouldClearFeed(true, DeletionInfo::ForAllHistory()));
  EXPECT_FALSE(
      ShouldClearFeed(true, DeletionInfo::ForUrls(
                                {
                                    history::URLRow(GURL("http://url1")),
                                    history::URLRow(GURL("http://url2")),
                                },
                                /*favicon_urls=*/{})));
  EXPECT_FALSE(ShouldClearFeed(false, DeletionInfo::ForAllHistory()));
}

}  // namespace

}  // namespace internal
}  // namespace feed
