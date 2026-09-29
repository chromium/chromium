// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include "base/test/scoped_feature_list.h"
#include "build/android_buildflags.h"
#include "components/search/ntp_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search {

namespace {

TEST(SearchTest, EmbeddedSearchAPIEnabledWithFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpEnableInstantApiAndroid);
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
#else
  EXPECT_FALSE(IsInstantExtendedAPIEnabled());
#endif
}

TEST(SearchTest, EmbeddedSearchAPIDisabledWithoutFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(ntp_features::kNtpEnableInstantApiAndroid);
  EXPECT_FALSE(IsInstantExtendedAPIEnabled());
}

}  // namespace

}  // namespace search
