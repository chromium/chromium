// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/hats_service.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class HatsForceEnabledTest : public testing::Test {
 public:
  HatsForceEnabledTest() {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kHappinessTrackingSurveysForDesktop, true);
    // I'm passing in a null pointer for profile for now.
    hats_service_ = std::make_unique<HatsService>(nullptr);
  }

 protected:
  std::unique_ptr<HatsService> hats_service_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HatsForceEnabledTest, ParamsWithAForcedFlagTest) {
  ASSERT_EQ(1, hats_service_->hats_finch_config_.probability);
}
