// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/most_visited_tiles_user.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

class MostVisitedTilesUserTest : public DefaultModelTestBase {
 public:
  MostVisitedTilesUserTest()
      : DefaultModelTestBase(std::make_unique<MostVisitedTilesUser>()) {}
  ~MostVisitedTilesUserTest() override = default;
};

TEST_F(MostVisitedTilesUserTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(MostVisitedTilesUserTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ExpectClassifierResults({10}, {"High"});
  ExpectClassifierResults({4}, {"Medium"});
  ExpectClassifierResults({2}, {"Low"});
  ExpectClassifierResults({1}, {"Low"});
  ExpectClassifierResults({0}, {"None"});
}

}  // namespace segmentation_platform
