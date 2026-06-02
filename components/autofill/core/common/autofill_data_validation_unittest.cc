// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_data_validation.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Tests that `IsValidFormFields` correctly identifies fields with unique global
// IDs as valid.
TEST(AutofillDataValidationTest, UniqueFieldGlobalIdsAreValid) {
  test::AutofillUnitTestEnvironment env;

  constexpr size_t kFieldsCount = 10;
  std::vector<FormFieldData> fields(kFieldsCount);
  for (FormFieldData& field_data : fields) {
    field_data.set_host_frame(env.NextLocalFrameToken());
    field_data.set_renderer_id(env.NextFieldRendererId());
  }

  EXPECT_TRUE(IsValidFormFields(fields));
}

// Tests that `IsValidFormFields` correctly identifies fields with duplicate
// global IDs as invalid.
TEST(AutofillDataValidationTest, DuplicateFieldGlobalIdsAreInvalid) {
  test::AutofillUnitTestEnvironment env;

  constexpr size_t kFieldsCount = 10;
  std::vector<FormFieldData> fields(kFieldsCount);
  for (FormFieldData& field_data : fields) {
    field_data.set_host_frame(env.NextLocalFrameToken());
    field_data.set_renderer_id(env.NextFieldRendererId());
  }
  fields.push_back(fields.front());

  EXPECT_FALSE(IsValidFormFields(fields));
}

// Tests that a form without duplicates of global IDs in its fields is
// identified as valid by `IsValidFormData`.
TEST(AutofillDataValidationTest, FormDataWithUniqueFieldGlobalIdsAreValid) {
  test::AutofillUnitTestEnvironment env;

  FormData form_data = test::CreateTestSignupFormData();

  EXPECT_GE(form_data.fields().size(), 3ul);
  EXPECT_TRUE(IsValidFormData(form_data));
}

// Tests that a form without duplicates of global IDs in its fields is
// identified as valid by `IsValidFormData`.
TEST(AutofillDataValidationTest,
     FormDataWithDuplicateFieldGlobalIdsAreInvalid) {
  test::AutofillUnitTestEnvironment env;

  FormData form_data = test::CreateTestSignupFormData();
  // Insert one duplicate field.
  std::vector<FormFieldData> fields = form_data.ExtractFields();
  ASSERT_FALSE(fields.empty());
  fields.push_back(fields.front());
  form_data.set_fields(std::move(fields));

  EXPECT_FALSE(IsValidFormData(form_data));
}

// Tests that metric "Autofill.FormData.Fields.DuplicateGlobalIdFound" works as
// intended in the context of `IsValidFormData`.
TEST(AutofillDataValidationTest, VerifyDuplicateGlobalIdUmaMetric) {
  test::AutofillUnitTestEnvironment env;

  FormData form_data = test::CreateTestSignupFormData();

  {
    base::HistogramTester histogram_tester;

    // Trigger no duplicate global ID found
    ASSERT_TRUE(IsValidFormData(form_data));

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormData.Fields.DuplicateGlobalIdFound"),
        base::BucketsAre(base::Bucket{true, 0}, base::Bucket{false, 1}));
  }

  std::vector<FormFieldData> fields = form_data.ExtractFields();
  ASSERT_FALSE(fields.empty());
  fields.push_back(fields.front());
  form_data.set_fields(std::move(fields));

  {
    base::HistogramTester histogram_tester;

    // Trigger duplicate global ID found
    ASSERT_FALSE(IsValidFormData(form_data));

    EXPECT_THAT(
        histogram_tester.GetAllSamples(
            "Autofill.FormData.Fields.DuplicateGlobalIdFound"),
        base::BucketsAre(base::Bucket{true, 1}, base::Bucket{false, 0}));
  }
}

}  // namespace

}  // namespace autofill
