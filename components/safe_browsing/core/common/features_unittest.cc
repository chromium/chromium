// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/features.h"

#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(SafeBrowsingFeatures, ClientSideDetectionTagDefault) {
  EXPECT_EQ(GetClientSideDetectionTag(), "default");
}

TEST(SafeBrowsingFeatures, ClientSideDetectionTagAllUsers) {
  base::test::ScopedFeatureList feature_list;
  base::test::ScopedFeatureList::FeatureAndParams all_users_feature(
      kClientSideDetectionModelTag, {{"reporter_omaha_tag", "all_users_tag"}});
  feature_list.InitWithFeaturesAndParameters({all_users_feature}, {});
  EXPECT_EQ(GetClientSideDetectionTag(), "all_users_tag");
}

TEST(SafeBrowsingFeatures, ClientSideDetectionTagHighMemory) {
  // One case where the user has enough memory
  {
    base::test::ScopedFeatureList::FeatureAndParams high_memory_feature(
        kClientSideDetectionModelHighMemoryTag,
        {{"reporter_omaha_tag", "high_memory_tag"},
         {"memory_threshold_mb",
          base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() -
                               1)}});

    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters({high_memory_feature}, {});
    EXPECT_EQ(GetClientSideDetectionTag(), "high_memory_tag");
  }

  // One case where the user does not have enough memory
  {
    base::test::ScopedFeatureList::FeatureAndParams high_memory_feature(
        kClientSideDetectionModelHighMemoryTag,
        {{"reporter_omaha_tag", "high_memory_tag"},
         {"memory_threshold_mb",
          base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() +
                               1)}});

    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters({high_memory_feature}, {});
    EXPECT_EQ(GetClientSideDetectionTag(), "default");
  }
}

TEST(SafeBrowsingFeatures, ClientSideDetectionTagAllUsersAndHighMemory) {
  // One case where the user has enough memory
  {
    base::test::ScopedFeatureList::FeatureAndParams all_users_feature(
        kClientSideDetectionModelTag,
        {{"reporter_omaha_tag", "all_users_tag"}});
    base::test::ScopedFeatureList::FeatureAndParams high_memory_feature(
        kClientSideDetectionModelHighMemoryTag,
        {{"reporter_omaha_tag", "high_memory_tag"},
         {"memory_threshold_mb",
          base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() -
                               1)}});

    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {all_users_feature, high_memory_feature}, {});
    EXPECT_EQ(GetClientSideDetectionTag(), "all_users_tag");
  }

  // One case where the user does not have enough memory
  {
    base::test::ScopedFeatureList::FeatureAndParams all_users_feature(
        kClientSideDetectionModelTag,
        {{"reporter_omaha_tag", "all_users_tag"}});
    base::test::ScopedFeatureList::FeatureAndParams high_memory_feature(
        kClientSideDetectionModelHighMemoryTag,
        {{"reporter_omaha_tag", "high_memory_tag"},
         {"memory_threshold_mb",
          base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB() +
                               1)}});

    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {all_users_feature, high_memory_feature}, {});
    EXPECT_EQ(GetClientSideDetectionTag(), "all_users_tag");
  }
}

}  // namespace safe_browsing
