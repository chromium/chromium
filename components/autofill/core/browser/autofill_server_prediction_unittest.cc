// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_server_prediction.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::Matcher;
using ::testing::Property;

Matcher<FieldPrediction> EqualsPrediction(FieldType prediction) {
  return AllOf(Property("type", &FieldPrediction::type, prediction),
               Property("source", &FieldPrediction::source,
                        FieldPrediction::SOURCE_AUTOFILL_DEFAULT));
}

class AutofillTypeServerPredictionTest : public testing::Test {
 private:
  test::AutofillUnitTestEnvironment autofill_environment_;
};

TEST_F(AutofillTypeServerPredictionTest, PredictionFromAutofillField) {
  AutofillField field = AutofillField(test::CreateTestFormField(
      "label", "name", "value", /*type=*/FormControlType::kInputText));
  field.set_server_predictions({test::CreateFieldPrediction(EMAIL_ADDRESS),
                                test::CreateFieldPrediction(USERNAME)});

  AutofillServerPrediction prediction(field);
  EXPECT_THAT(
      prediction.server_predictions,
      ElementsAre(EqualsPrediction(EMAIL_ADDRESS), EqualsPrediction(USERNAME)));
}

}  // namespace
}  // namespace autofill
