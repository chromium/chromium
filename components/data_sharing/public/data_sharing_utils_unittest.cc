// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/public/data_sharing_utils.h"

#include "components/data_sharing/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {

namespace {
const char kGroupId[] = "/?-group_id";
const char kTokenBlob[] = "/?-_token";

TEST(DataSharingUtilsTest, ParseAndInterceptDataSharingURL) {
  GURL url = GURL(data_sharing::features::kDataSharingURL.Get() +
                  "?g=" + kGroupId + "&t=" + kTokenBlob);

  ParseUrlResult result = DataSharingUtils::ParseDataSharingUrl(url);

  // Verify valid path.
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kGroupId, result.value().group_id.value());
  EXPECT_EQ(kTokenBlob, result.value().access_token);
  EXPECT_TRUE(DataSharingUtils::ShouldInterceptNavigationForShareURL(url));

  // Verify host/path error.
  std::string invalid = "https://www.test.com/";
  url = GURL(invalid + "?g=" + kGroupId + "&t=" + kTokenBlob);
  result = DataSharingUtils::ParseDataSharingUrl(url);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParseUrlStatus::kHostOrPathMismatchFailure);
  EXPECT_FALSE(DataSharingUtils::ShouldInterceptNavigationForShareURL(url));

  // Verify query missing error.
  url = GURL(data_sharing::features::kDataSharingURL.Get() +
             "?access_token=" + kGroupId);
  result = DataSharingUtils::ParseDataSharingUrl(url);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ParseUrlStatus::kQueryMissingFailure);
  EXPECT_TRUE(DataSharingUtils::ShouldInterceptNavigationForShareURL(url));

  // Verify access token missing is ok.
  url = GURL(data_sharing::features::kDataSharingURL.Get() + "?g=" + kGroupId);
  result = DataSharingUtils::ParseDataSharingUrl(url);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(kGroupId, result.value().group_id.value());
  EXPECT_EQ("", result.value().access_token);
  EXPECT_TRUE(DataSharingUtils::ShouldInterceptNavigationForShareURL(url));
}

}  // namespace
}  // namespace data_sharing
