// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change/change_password_generation_util.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

using ::testing::_;
using ::testing::Return;

constexpr char kTestUrl[] = "https://example.com/change-password";

class MockPasswordGenerationFrameHelper : public PasswordGenerationFrameHelper {
 public:
  using PasswordGenerationFrameHelper::PasswordGenerationFrameHelper;
  MOCK_METHOD(autofill::PasswordRequirementsSpec,
              GetPasswordRequirementsSpec,
              (const GURL&,
               autofill::password_generation::PasswordGenerationType,
               autofill::FormSignature,
               autofill::FieldSignature,
               uint64_t),
              (override));
};

autofill::FormFieldData CreatePasswordField(const char* name,
                                            uint32_t renderer_id,
                                            uint64_t max_length) {
  autofill::FormFieldData field = autofill::test::CreateTestFormField(
      /*label=*/name, name, /*value=*/"",
      autofill::FormControlType::kInputPassword);
  field.set_renderer_id(autofill::FieldRendererId(renderer_id));
  field.set_max_length(max_length);
  return field;
}

class ChangePasswordGenerationUtilTest : public testing::Test {
 protected:
  MockPasswordGenerationFrameHelper& generation_helper() {
    return generation_helper_;
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  StubPasswordManagerClient client_;
  StubPasswordManagerDriver driver_;
  testing::NiceMock<MockPasswordGenerationFrameHelper> generation_helper_{
      &client_, &driver_};
};

TEST_F(ChangePasswordGenerationUtilTest, PrefersNewPasswordField) {
  PasswordForm form;
  form.url = GURL(kTestUrl);
  std::vector<autofill::FormFieldData> fields = {
      CreatePasswordField("current", 1, 20),
      CreatePasswordField("new", 2, 16),
      CreatePasswordField("confirm", 3, 24),
  };
  const autofill::FormFieldData expected_field = fields[1];
  form.form_data.set_fields(std::move(fields));
  form.password_element_renderer_id = autofill::FieldRendererId(1);
  form.new_password_element_renderer_id = autofill::FieldRendererId(2);
  form.confirmation_password_element_renderer_id = autofill::FieldRendererId(3);

  autofill::PasswordRequirementsSpec spec;
  spec.set_max_length(12);
  EXPECT_CALL(
      generation_helper(),
      GetPasswordRequirementsSpec(
          GURL(kTestUrl),
          autofill::password_generation::PasswordGenerationType::kAutomatic,
          autofill::CalculateFormSignature(form.form_data),
          autofill::CalculateFieldSignatureForField(expected_field), 16))
      .WillOnce(Return(spec));

  ChangePasswordGenerationResult result =
      GeneratePasswordForChangePasswordForm(form, generation_helper());
  EXPECT_EQ(result.password.size(), 12u);
  EXPECT_EQ(result.spec.max_length(), 12u);
}

TEST_F(ChangePasswordGenerationUtilTest, FallsBackToCurrentPasswordField) {
  PasswordForm form;
  form.url = GURL(kTestUrl);
  std::vector<autofill::FormFieldData> fields = {
      CreatePasswordField("current", 1, 20),
  };
  const autofill::FormFieldData expected_field = fields[0];
  form.form_data.set_fields(std::move(fields));
  form.password_element_renderer_id = autofill::FieldRendererId(1);

  EXPECT_CALL(
      generation_helper(),
      GetPasswordRequirementsSpec(
          _, _, _, autofill::CalculateFieldSignatureForField(expected_field),
          20));

  ChangePasswordGenerationResult result =
      GeneratePasswordForChangePasswordForm(form, generation_helper());
  EXPECT_FALSE(result.password.empty());
}

TEST_F(ChangePasswordGenerationUtilTest,
       UsesDomainWideRequirementsWithoutPasswordField) {
  PasswordForm form;
  form.url = GURL(kTestUrl);

  EXPECT_CALL(generation_helper(),
              GetPasswordRequirementsSpec(_, _, _, autofill::FieldSignature(),
                                          /*max_length=*/0));

  ChangePasswordGenerationResult result =
      GeneratePasswordForChangePasswordForm(form, generation_helper());
  EXPECT_FALSE(result.password.empty());
}

}  // namespace
}  // namespace password_manager
