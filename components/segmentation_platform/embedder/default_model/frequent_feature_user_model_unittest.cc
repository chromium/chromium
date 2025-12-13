// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/frequent_feature_user_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/constants.h"

namespace segmentation_platform {

using Feature = FrequentFeatureUserModel::Feature;

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
  ModelProvider::Request inputs1(Feature::kFeatureCount, 0);
  ExpectClassifierResults(inputs1, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs1, /*expected_error=*/false,
                           /*expected_result=*/{0});

  ModelProvider::Request inputs2(Feature::kFeatureCount, 0);
  inputs2[Feature::kFeatureMobileMenuAddToBookmarks] = 1;
  ExpectClassifierResults(inputs2, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs2,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs3(Feature::kFeatureCount, 0);
  inputs3[Feature::kFeatureSuggestionsContentOpened] = 2;
  ExpectClassifierResults(inputs3, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs3,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs4(Feature::kFeatureCount, 0);
  inputs4[Feature::kFeatureOmniboxUrl] = 1;
  ExpectClassifierResults(inputs4, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs4,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs5(Feature::kFeatureCount, 0);
  inputs5[Feature::kFeaturePasswordManagerAutofilled] = 3;
  inputs5[Feature::kFeatureOmniboxUrl] = 1;
  ExpectClassifierResults(inputs5, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs5,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs6(Feature::kFeatureCount, 0);
  inputs6[Feature::kFeatureOmniboxSearch] = 1;
  ExpectClassifierResults(inputs6, {kLegacyNegativeLabel});
  ExpectExecutionWithInput(inputs6,
                           /*expected_error=*/false, /*expected_result=*/{0});

  ModelProvider::Request inputs7(Feature::kFeatureCount, 0);
  inputs7[Feature::kFeatureAutofillCreditCard] = 1;
  inputs7[Feature::kFeatureOmniboxSearch] = 1;
  ExpectClassifierResults(inputs7, {segment_label});
  ExpectExecutionWithInput(inputs7,
                           /*expected_error=*/false, /*expected_result=*/{1});

  ModelProvider::Request inputs8(Feature::kFeatureCount, 0);
  inputs8[Feature::kFeatureOmniboxUrl] = 1;
  inputs8[Feature::kFeatureOmniboxSearch] = 1;
  ExpectClassifierResults(inputs8, {segment_label});
  ExpectExecutionWithInput(inputs8,
                           /*expected_error=*/false, /*expected_result=*/{1});

  ModelProvider::Request inputs9(Feature::kFeatureCount, 0);
  inputs9[Feature::kFeatureSuggestionsContentOpened] = 1;
  inputs9[Feature::kFeatureMobileBookmarkManagerEntryOpened] = 2;
  inputs9[Feature::kFeatureOmniboxSearch] = 1;
  ExpectClassifierResults(inputs9, {segment_label});
  ExpectExecutionWithInput(inputs9,
                           /*expected_error=*/false, /*expected_result=*/{1});
}

}  // namespace segmentation_platform
