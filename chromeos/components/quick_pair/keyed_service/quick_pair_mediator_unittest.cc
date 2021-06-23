// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_pair/keyed_service/quick_pair_mediator.h"

#include <memory>

#include "chromeos/components/quick_pair/feature_status_tracker/mock_quick_pair_feature_status_tracker.h"
#include "chromeos/components/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_pair {

class MediatorTest : public testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<FeatureStatusTracker> tracker =
        std::make_unique<MockFeatureStatusTracker>();
    feature_status_tracker_ =
        static_cast<MockFeatureStatusTracker*>(tracker.get());

    EXPECT_CALL(*feature_status_tracker_, AddObserver);
    EXPECT_CALL(*feature_status_tracker_, IsFastPairEnabled);

    mediator_ = std::make_unique<Mediator>(std::move(tracker));
  }

  void TearDown() override {
    EXPECT_CALL(*feature_status_tracker_, RemoveObserver(mediator_.get()));
  }

 protected:
  MockFeatureStatusTracker* feature_status_tracker_;
  std::unique_ptr<Mediator> mediator_;
};

TEST_F(MediatorTest, SetupAndTeardownMakesExpectedCalls) {
  // Blank test to explicitly test SetUp and TearDown. Can be removed once
  // more tests are added which will test the same logic.
}

}  // namespace quick_pair
}  // namespace chromeos
