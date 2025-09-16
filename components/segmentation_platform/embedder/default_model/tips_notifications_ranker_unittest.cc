// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/segmentation_platform/embedder/default_model/tips_notifications_ranker.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace segmentation_platform {
class TipsNotificationsRankerTest : public DefaultModelTestBase {
 public:
  TipsNotificationsRankerTest()
      : DefaultModelTestBase(std::make_unique<TipsNotificationsRanker>()) {}
  ~TipsNotificationsRankerTest() override = default;
  void SetUp() override { DefaultModelTestBase::SetUp(); }
  void TearDown() override { DefaultModelTestBase::TearDown(); }
};
TEST_F(TipsNotificationsRankerTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}
TEST_F(TipsNotificationsRankerTest, ExecuteModelWithInputForAllModules) {
  ExpectInitAndFetchModel();
  ExpectClassifierResults({0, 0, 0, 0}, {});
}
}  // namespace segmentation_platform
