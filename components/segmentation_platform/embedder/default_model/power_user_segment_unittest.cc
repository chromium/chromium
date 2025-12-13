// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/power_user_segment.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"

namespace segmentation_platform {

using Feature = PowerUserSegment::Feature;
using Label = PowerUserSegment::Label;

class PowerUserModelTest : public DefaultModelTestBase {
 public:
  PowerUserModelTest()
      : DefaultModelTestBase(std::make_unique<PowerUserSegment>()) {}
  ~PowerUserModelTest() override = default;
};

TEST_F(PowerUserModelTest, InitAndFetchModel) {
  ExpectInitAndFetchModel();
}

TEST_F(PowerUserModelTest, ExecuteModelWithInput) {
  ExpectInitAndFetchModel();
  ASSERT_TRUE(fetched_metadata_);

  std::string subsegment_key = GetSubsegmentKey(kPowerUserKey);
  ModelProvider::Request input(27, 0);
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           {Label::kLabelNone});
  ExpectClassifierResults(input, {"None"});

  input[Feature::kFeatureMobileMenuDownloadManager] = 3;
  input[Feature::kFeatureMobileMenuShare] = 4;
  input[Feature::kFeatureMobileMenuAllBookmarks] = 4;
  input[Feature::kFeatureMobileOmniboxVoiceSearch] = 20;
  ExpectExecutionWithInput(input, /*expected_error=*/false, {Label::kLabelLow});
  ExpectClassifierResults(input, {"Low"});

  input[Feature::kFeatureMediaControlsCast] = 2;
  input[Feature::kFeatureAutofillKeyMetricsFillingAcceptanceAddress] = 5;
  input[Feature::kFeatureAndroidPhotoPickerDiaglogAction] = 6;
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           {Label::kLabelMedium});
  ExpectClassifierResults(input, {"Medium"});

  input[Feature::kFeatureSessionTotalDuration] = 20 * 60 * 1000;
  input[Feature::kFeatureMediaOutputStreamDuration] = 60000;
  input[Feature::kFeatureDataUseTrafficSizeUserUpstreamForegroundNotCellular] =
      50000;
  ExpectExecutionWithInput(input, /*expected_error=*/false,
                           {Label::kLabelHigh});
  ExpectClassifierResults(input, {"High"});

  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{}));
  EXPECT_FALSE(ExecuteWithInput(/*inputs=*/{1, 2}));
}

}  // namespace segmentation_platform
