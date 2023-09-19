// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/frequent_feature_user_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/constants.h"

namespace segmentation_platform {

class FrequentFeatureUserModelTest : public DefaultModelTestBase {
 public:
  FrequentFeatureUserModelTest()
      : DefaultModelTestBase(std::make_unique<FrequentFeatureUserModel>()) {}
  ~FrequentFeatureUserModelTest() override = default;
};

TEST_F(FrequentFeatureUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(FrequentFeatureUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();

  const std::string segment_label =
      SegmentIdToHistogramVariant(SegmentId::FREQUENT_FEATURE_USER_SEGMENT);
  ModelProvider::Request inputs1{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  ExpectClassifierResults(inputs1, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs1, /*expected_error=*/false,
                           /*expected_result=*/{0});

  ModelProvider::Request inputs2{1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  ExpectClassifierResults(inputs2, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs2,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs3{0, 0, 2, 0, 0, 0, 0, 0, 0, 0};
  ExpectClassifierResults(inputs3, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs3,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs4{0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
  ExpectClassifierResults(inputs4, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs4,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs5{0, 0, 0, 0, 0, 0, 3, 0, 1, 0};
  ExpectClassifierResults(inputs5, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs5,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs6{0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
  ExpectClassifierResults(inputs6, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs6,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs7{0, 0, 0, 0, 0, 1, 0, 0, 0, 1};
  ExpectClassifierResults(inputs7, {segment_label});
  ExpectExecutionWithInput(inputs7,
                           /*expected_error=*/false, /*expected_result=*/{1});

  ModelProvider::Request inputs8{0, 0, 0, 0, 0, 0, 0, 0, 1, 1};
  ExpectClassifierResults(inputs8, {segment_label});
  ExpectExecutionWithInput(inputs8,
                           /*expected_error=*/false, /*expected_result=*/{1});

  ModelProvider::Request inputs9{0, 0, 1, 0, 2, 0, 0, 0, 0, 1};
  ExpectClassifierResults(inputs9, {segment_label});
  ExpectExecutionWithInput(inputs9,
                           /*expected_error=*/false, /*expected_result=*/{1});
}

}  // namespace segmentation_platform
