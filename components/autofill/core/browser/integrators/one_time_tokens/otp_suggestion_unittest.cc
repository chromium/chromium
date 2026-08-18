// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_suggestion.h"

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::autofill::test::FormDescription;
using ::autofill::test::GetHeuristicTypes;
using ::autofill::test::GetServerTypes;

namespace autofill {

class OtpSuggestionTest : public testing::Test {
 public:
  OtpSuggestionTest() = default;

  void SetUp() override {
    FormDescription form_description = {
        .fields = {
            {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp1"},
            {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp2"},
            {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp3"},
            {.server_type = ONE_TIME_CODE, .label = u"OTP", .name = u"otp4"},
            {.server_type = UNKNOWN_TYPE, .label = u"OTP", .name = u"otp5"},
        }};
    FormData form = test::GetFormData(form_description);
    form_structure_ = std::make_unique<FormStructure>(form);
    test_api(*form_structure_)
        .SetFieldTypes(GetHeuristicTypes(form_description),
                       GetServerTypes(form_description));
  }

  FieldGlobalId field_id(size_t idx) {
    return form_structure_->fields()[idx]->global_id();
  }

 protected:
  std::unique_ptr<FormStructure> form_structure_;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// If the OTP value is longer than the number of detected fields, it should be
// filled completely into the trigger field.
TEST_F(OtpSuggestionTest, OtpLongerThanNumberOfDetectedFields) {
  OtpFillData fill_data = CreateFillDataForOtpSuggestion(
      *form_structure_, *form_structure_->fields()[0], u"12345");
  EXPECT_EQ(1u, fill_data.size());
  EXPECT_EQ(u"12345", fill_data.at(field_id(0)));
}

// If the length of the OTP value matches the number of detected OTP fields, it
// should be split across all fields.
TEST_F(OtpSuggestionTest, OtpLengthEqualsNumberOfDetectedFields) {
  OtpFillData fill_data = CreateFillDataForOtpSuggestion(
      *form_structure_, *form_structure_->fields()[0], u"1234");
  EXPECT_EQ(4u, fill_data.size());
  EXPECT_EQ(u"1", fill_data.at(field_id(0)));
  EXPECT_EQ(u"2", fill_data.at(field_id(1)));
  EXPECT_EQ(u"3", fill_data.at(field_id(2)));
  EXPECT_EQ(u"4", fill_data.at(field_id(3)));
}

// If the length of the OTP value matches the number of detected OTP fields, it
// should be split across all fields even if the trigger field is not the first
// field.
TEST_F(OtpSuggestionTest,
       OtpLengthEqualsNumberOfDetectedFields_DifferentTriggerField) {
  OtpFillData fill_data = CreateFillDataForOtpSuggestion(
      *form_structure_, *form_structure_->fields()[3], u"1234");
  // The delta to the previous test OtpLengthEqualsNumberOfDetectedFields is
  // that the trigger field is the last field.
  EXPECT_EQ(4u, fill_data.size());
  EXPECT_EQ(u"1", fill_data.at(field_id(0)));
  EXPECT_EQ(u"2", fill_data.at(field_id(1)));
  EXPECT_EQ(u"3", fill_data.at(field_id(2)));
  EXPECT_EQ(u"4", fill_data.at(field_id(3)));
}

// If the length of the OTP value is smaller than the number of detected fields
// and there are enough fields to fill the value starting from the middle field,
// they should be filled starting at the trigger field. Previous fields may not
// be detected as invisible.
TEST_F(OtpSuggestionTest, OtpLengthSmallerThanNumberOfDetectedFields) {
  OtpFillData fill_data = CreateFillDataForOtpSuggestion(
      *form_structure_, *form_structure_->fields()[0], u"12");
  EXPECT_EQ(2u, fill_data.size());
  EXPECT_EQ(u"1", fill_data.at(field_id(0)));
  EXPECT_EQ(u"2", fill_data.at(field_id(1)));
}

// If the length of the OTP value is smaller than the number of detected fields
// but there are NOT enough fields to fill the value starting from the middle
// field, the entire OTP should be filled into the trigger field.
TEST_F(OtpSuggestionTest,
       OtpLengthSmallerThanNumberOfDetectedFields_NotEnoughFields) {
  OtpFillData fill_data = CreateFillDataForOtpSuggestion(
      *form_structure_, *form_structure_->fields()[3], u"12");
  EXPECT_EQ(1u, fill_data.size());
  EXPECT_EQ(u"12", fill_data.at(field_id(3)));
}

// If the OTP field is not among currently detected OTP fields (e.g. due to a
// fallback or because the form changed), fill the entire OTP into the trigger
// field.
TEST_F(OtpSuggestionTest, TriggerFieldIsNotOtpFieldAnymore) {
  OtpFillData fill_data = CreateFillDataForOtpSuggestion(
      *form_structure_, *form_structure_->fields()[4], u"1234");
  EXPECT_EQ(1u, fill_data.size());
  EXPECT_EQ(u"1234", fill_data.at(field_id(4)));
}

// If a form contains visible OTP digit fields AND a hidden OTP field,
// the hidden field is counted as an OTP field in the current implementation.
// When filling is triggered from the second field, the OTP value is shifted by
// one field, resulting in the first digit being duplicated in the second box.
TEST_F(OtpSuggestionTest, DuplicateFirstDigitWithHiddenField) {
  FormDescription form_description = {.fields = {
                                          {.server_type = ONE_TIME_CODE,
                                           .is_focusable = true,
                                           .is_visible = true,
                                           .name = u"digit1"},
                                          {.server_type = ONE_TIME_CODE,
                                           .is_focusable = true,
                                           .is_visible = true,
                                           .name = u"digit2"},
                                          {.server_type = ONE_TIME_CODE,
                                           .is_focusable = true,
                                           .is_visible = true,
                                           .name = u"digit3"},
                                          {.server_type = ONE_TIME_CODE,
                                           .is_focusable = true,
                                           .is_visible = true,
                                           .name = u"digit4"},
                                          {.server_type = ONE_TIME_CODE,
                                           .is_focusable = true,
                                           .is_visible = true,
                                           .name = u"digit5"},
                                          {.server_type = ONE_TIME_CODE,
                                           .is_focusable = true,
                                           .is_visible = true,
                                           .name = u"digit6"},
                                          {.server_type = ONE_TIME_CODE,
                                           .is_focusable = false,
                                           .is_visible = false,
                                           .name = u"hidden_tan"},
                                      }};
  FormData form = test::GetFormData(form_description);
  auto form_structure = std::make_unique<FormStructure>(form);
  test_api(*form_structure)
      .SetFieldTypes(GetHeuristicTypes(form_description),
                     GetServerTypes(form_description));

  // Trigger filling with focus on the second box (`digit2`).
  const AutofillField& trigger_field = *form_structure->fields()[1];
  OtpFillData fill_data =
      CreateFillDataForOtpSuggestion(*form_structure, trigger_field, u"123456");

  EXPECT_EQ(6u, fill_data.size());
  // First digit is empty.
  EXPECT_FALSE(fill_data.contains(form_structure->fields()[0]->global_id()));
  EXPECT_EQ(u"1", fill_data.at(form_structure->fields()[1]->global_id()));
  EXPECT_EQ(u"2", fill_data.at(form_structure->fields()[2]->global_id()));
  EXPECT_EQ(u"3", fill_data.at(form_structure->fields()[3]->global_id()));
  EXPECT_EQ(u"4", fill_data.at(form_structure->fields()[4]->global_id()));
  EXPECT_EQ(u"5", fill_data.at(form_structure->fields()[5]->global_id()));
  // Last digit is inserted into the hidden field.
  EXPECT_EQ(u"6", fill_data.at(form_structure->fields()[6]->global_id()));
}

}  // namespace autofill
