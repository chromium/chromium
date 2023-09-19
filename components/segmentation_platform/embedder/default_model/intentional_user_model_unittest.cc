// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/intentional_user_model.h"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "components/segmentation_platform/public/constants.h"

namespace segmentation_platform {

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
  // Test with empty input.
  ExpectExecutionWithInput(/*inputs=*/{}, /*expected_error=*/true,
                           /*expected_result=*/{0});

  // Test with more inputs than expected.
  ExpectExecutionWithInput(/*inputs=*/{12, 21}, /*expected_error=*/true,
                           /*expected_result=*/{0});

  // If Chrome hasn't been launched from its main launcher icon then the user is
  // not intentional.
  ExpectExecutionWithInput(/*inputs=*/{0}, /*expected_error=*/false,
                           /*expected_result=*/{0});

  ExpectExecutionWithInput(/*inputs=*/{1}, /*expected_error=*/false,
                           /*expected_result=*/{0});

  // If chrome was launched at least twice from its main laincher icon then the
  // user is intentional.
  ExpectExecutionWithInput(/*inputs=*/{2}, /*expected_error=*/false,
                           /*expected_result=*/{1});

  ExpectExecutionWithInput(/*inputs=*/{10}, /*expected_error=*/false,
                           /*expected_result=*/{1});
}

TEST_F(IntentionalUserModelTest, TestLabels) {
  ExpectInitAndFetchModel();

  // If Chrome hasn't been launched from its main launcher icon then the user is
  // not intentional.
  ExpectClassifierResults({0}, {kLegacyNegativeLabel});
  ExpectClassifierResults({1}, {kLegacyNegativeLabel});

  // If chrome was launched at least twice from its main laincher icon then the
  // user is intentional.
  ExpectClassifierResults(
      {2}, {SegmentIdToHistogramVariant(SegmentId::INTENTIONAL_USER_SEGMENT)});
  ExpectClassifierResults(
      {10}, {SegmentIdToHistogramVariant(SegmentId::INTENTIONAL_USER_SEGMENT)});
}

}  // namespace segmentation_platform
