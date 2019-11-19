// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_feature.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

TEST(OfflinePageFeatureTest, OffliningRecentPages) {
  // Enabled by default.
  EXPECT_TRUE(offline_pages::IsOffliningRecentPagesEnabled());

  // Check if helper method works correctly when the features is disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kOffliningRecentPagesFeature);
  EXPECT_FALSE(offline_pages::IsOffliningRecentPagesEnabled());
}

TEST(OfflinePageFeatureTest, OfflinePagesLivePageSharing) {
  // Disabled by default.
  EXPECT_FALSE(
      base::FeatureList::IsEnabled(kOfflinePagesLivePageSharingFeature));

  // Check if helper method works correctly when the feature is disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kOfflinePagesLivePageSharingFeature);
  EXPECT_TRUE(offline_pages::IsOfflinePagesLivePageSharingEnabled());
}

TEST(OfflinePageFeatureTest, OfflinePagesLoadSignalCollecting) {
  // Disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflinePagesLoadSignalCollectingEnabled());

  // Check if helper method works correctly when the features is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesLoadSignalCollectingFeature);
  EXPECT_TRUE(offline_pages::IsOfflinePagesLoadSignalCollectingEnabled());
}

TEST(OfflinePageFeatureTest, OfflinePagesPrefetching) {
  // Enabled by default.
  EXPECT_TRUE(offline_pages::IsPrefetchingOfflinePagesEnabled());

  // Check if helper method works correctly when the features is disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kPrefetchingOfflinePagesFeature);
  EXPECT_FALSE(offline_pages::IsPrefetchingOfflinePagesEnabled());
}

TEST(OfflinePageFeatureTest, OfflinePagesInDownloadHomeOpenInCct) {
  // Enabled by default.
  EXPECT_TRUE(offline_pages::ShouldOfflinePagesInDownloadHomeOpenInCct());

  // Check if helper method works correctly when the features is disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kOfflinePagesInDownloadHomeOpenInCctFeature);
  EXPECT_FALSE(offline_pages::ShouldOfflinePagesInDownloadHomeOpenInCct());
}

TEST(OfflinePageFeatureTest, OfflinePagesDescriptiveFailStatus) {
  // Disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflinePagesDescriptiveFailStatusEnabled());

  // Check if helper method works correctly when the features is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesDescriptiveFailStatusFeature);
  EXPECT_TRUE(offline_pages::IsOfflinePagesDescriptiveFailStatusEnabled());
}

TEST(OfflinePageFeatureTest, OfflinePagesDescriptivePendingStatus) {
  // Enabled by default.
  EXPECT_TRUE(offline_pages::IsOfflinePagesDescriptivePendingStatusEnabled());

  // Check if helper method works correctly when the features is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kOfflinePagesDescriptivePendingStatusFeature);
  EXPECT_FALSE(offline_pages::IsOfflinePagesDescriptivePendingStatusEnabled());
}

TEST(OfflinePageFeatureTest, AlternateDinoPage) {
  // Disabled by default.
  EXPECT_FALSE(offline_pages::ShouldShowAlternateDinoPage());

  // Check if helper method works correctly when the features is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kOfflinePagesShowAlternateDinoPageFeature);
  EXPECT_TRUE(offline_pages::ShouldShowAlternateDinoPage());
}

}  // namespace offline_pages
