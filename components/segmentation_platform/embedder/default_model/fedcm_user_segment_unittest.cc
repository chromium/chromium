// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/fedcm_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

const char kFedCmUserLoudLabel[] = "FedCmUserLoud";
const char kFedCmUserQuietLabel[] = "FedCmUserQuiet";

class FedCmUserModelTest : public DefaultModelTestBase {
 public:
  FedCmUserModelTest()
      : DefaultModelTestBase(std::make_unique<FedCmUserModel>()) {}
  ~FedCmUserModelTest() override = default;
};

TEST_F(FedCmUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(FedCmUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));

  // FedCM Loud UI.
  ExpectClassifierResults(/*input=*/{0, 0, 0, 0}, {kFedCmUserLoudLabel});
  ExpectClassifierResults(/*input=*/{1, 1, 0, 0}, {kFedCmUserLoudLabel});
  ExpectClassifierResults(/*input=*/{1, 0, 1, 0}, {kFedCmUserLoudLabel});
  ExpectClassifierResults(/*input=*/{1, 0, 3, 1}, {kFedCmUserLoudLabel});

  // FedCM Quiet UI.
  ExpectClassifierResults(/*input=*/{1, 0, 3, 0}, {kFedCmUserQuietLabel});
  ExpectClassifierResults(/*input=*/{2, 0, 5, 0}, {kFedCmUserQuietLabel});
}

}  // namespace segmentation_platform
