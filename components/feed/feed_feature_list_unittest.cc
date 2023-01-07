// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/feed_feature_list.h"
#include <sstream>

#include "base/callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/public/base/consent_level.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

TEST(FeedFeatureList, GetConsentLevelNeededForPersonalizedFeed) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kPersonalizeFeedNonSyncUsers);

    EXPECT_EQ(signin::ConsentLevel::kSignin,
              GetConsentLevelNeededForPersonalizedFeed());
  }
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(kPersonalizeFeedNonSyncUsers);

    EXPECT_EQ(signin::ConsentLevel::kSync,
              GetConsentLevelNeededForPersonalizedFeed());
  }
}

}  // namespace feed