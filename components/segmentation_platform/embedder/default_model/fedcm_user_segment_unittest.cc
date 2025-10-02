// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/fedcm_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

using Feature = FedCmUserModel::Feature;

const char kFedCmUserLoudLabel[] = "FedCmUserLoud";

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
  ExpectClassifierResults(ModelProvider::Request(Feature::kFeatureCount, 0),
                          {kFedCmUserLoudLabel});
  ModelProvider::Request input1(Feature::kFeatureCount, 0);
  input1[Feature::kFeatureAccountsDialogShown] = 1;
  input1[Feature::kFeatureRequestIdToken] = 1;
  ExpectClassifierResults(input1, {kFedCmUserLoudLabel});

  ModelProvider::Request input2(Feature::kFeatureCount, 0);
  input2[Feature::kFeatureAccountsDialogShown] = 1;
  input2[Feature::kFeatureCancelReason] = 1;
  ExpectClassifierResults(input2, {kFedCmUserLoudLabel});

  ModelProvider::Request input3(Feature::kFeatureCount, 0);
  input3[Feature::kFeatureAccountsDialogShown] = 1;
  input3[Feature::kFeatureCancelReason] = 3;
  input3[Feature::kFeatureIsSignInUser] = 1;
  ExpectClassifierResults(input3, {kFedCmUserLoudLabel});

  // All inputs should result in FedCM Loud UI.
  ModelProvider::Request input4(Feature::kFeatureCount, 0);
  input4[Feature::kFeatureAccountsDialogShown] = 1;
  input4[Feature::kFeatureCancelReason] = 3;
  ExpectClassifierResults(input4, {kFedCmUserLoudLabel});

  ModelProvider::Request input5(Feature::kFeatureCount, 0);
  input5[Feature::kFeatureAccountsDialogShown] = 2;
  input5[Feature::kFeatureCancelReason] = 5;
  ExpectClassifierResults(input5, {kFedCmUserLoudLabel});
}

}  // namespace segmentation_platform
