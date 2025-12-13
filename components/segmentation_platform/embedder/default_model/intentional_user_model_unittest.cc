// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/intentional_user_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/constants.h"

namespace segmentation_platform {
using Feature = IntentionalUserModel::Feature;

class IntentionalUserModelTest : public DefaultModelTestBase {
 public:
  IntentionalUserModelTest()
      : DefaultModelTestBase(std::make_unique<IntentionalUserModel>()) {}
  ~IntentionalUserModelTest() override = default;
};

TEST_F(IntentionalUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(IntentionalUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  // Test with empty input.
  ExpectExecutionWithInput(/*inputs=*/{}, /*expected_error=*/true,
                           /*expected_result=*/{0});

  // Test with more inputs than expected.
  ExpectExecutionWithInput(/*inputs=*/{12, 21}, /*expected_error=*/true,
                           /*expected_result=*/{0});

  // If Chrome hasn't been launched from its main launcher icon then the user is
  // not intentional.
  ModelProvider::Request input1(Feature::kFeatureCount, 0);
  input1[Feature::kFeatureLaunchCauseMainLauncherIcon] = 0;
  ExpectExecutionWithInput(input1, /*expected_error=*/false,
                           /*expected_result=*/{0});

  ModelProvider::Request input2(Feature::kFeatureCount, 0);
  input2[Feature::kFeatureLaunchCauseMainLauncherIcon] = 1;
  ExpectExecutionWithInput(input2, /*expected_error=*/false,
                           /*expected_result=*/{0});

  // If chrome was launched at least twice from its main laincher icon then the
  // user is intentional.
  ModelProvider::Request input3(Feature::kFeatureCount, 0);
  input3[Feature::kFeatureLaunchCauseMainLauncherIcon] = 2;
  ExpectExecutionWithInput(input3, /*expected_error=*/false,
                           /*expected_result=*/{1});

  ModelProvider::Request input4(Feature::kFeatureCount, 0);
  input4[Feature::kFeatureLaunchCauseMainLauncherIcon] = 10;
  ExpectExecutionWithInput(input4, /*expected_error=*/false,
                           /*expected_result=*/{1});
}

TEST_F(IntentionalUserModelTest, TestLabels) {
  ExpectInitAndFetchModel();

  // If Chrome hasn't been launched from its main launcher icon then the user is
  // not intentional.
  ModelProvider::Request input1(Feature::kFeatureCount, 0);
  input1[Feature::kFeatureLaunchCauseMainLauncherIcon] = 0;
  ExpectClassifierResults(input1, {kLegacyNegativeLabel});

  ModelProvider::Request input2(Feature::kFeatureCount, 0);
  input2[Feature::kFeatureLaunchCauseMainLauncherIcon] = 1;
  ExpectClassifierResults(input2, {kLegacyNegativeLabel});

  // If chrome was launched at least twice from its main laincher icon then the
  // user is intentional.
  ModelProvider::Request input3(Feature::kFeatureCount, 0);
  input3[Feature::kFeatureLaunchCauseMainLauncherIcon] = 2;
  ExpectClassifierResults(
      input3,
      {SegmentIdToHistogramVariant(SegmentId::INTENTIONAL_USER_SEGMENT)});

  ModelProvider::Request input4(Feature::kFeatureCount, 0);
  input4[Feature::kFeatureLaunchCauseMainLauncherIcon] = 10;
  ExpectClassifierResults(
      input4,
      {SegmentIdToHistogramVariant(SegmentId::INTENTIONAL_USER_SEGMENT)});
}

}  // namespace segmentation_platform
