// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/features.h"

#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(SafeBrowsingFeatures, ClientSideDetectionTagDefault) {
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(GetClientSideDetectionTag(), "android_1");
#else
  EXPECT_EQ(GetClientSideDetectionTag(), "desktop_1_flatbuffer");
#endif
}

TEST(SafeBrowsingFeatures, ClientSideDetectionTagAllUsers) {
  base::test::ScopedFeatureList feature_list;
  base::test::FeatureRefAndParams all_users_feature(
      kClientSideDetectionModelTag, {{"reporter_omaha_tag", "all_users_tag"}});
  feature_list.InitWithFeaturesAndParameters({all_users_feature}, {});
  EXPECT_EQ(GetClientSideDetectionTag(), "all_users_tag");
}

}  // namespace safe_browsing
