// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/otp_manager.h"

#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"

namespace password_manager {

namespace {

using autofill::FormData;

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;

  MOCK_METHOD(void, InformPasswordChangeServiceOfOtpPresent, (), (override));
};
}  // namespace

class OtpManagerTest : public testing::Test {
 public:
  OtpManagerTest() : otp_manager_(&mock_client_) {}

 protected:
  MockPasswordManagerClient mock_client_;
  OtpManager otp_manager_;

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
};

TEST_F(OtpManagerTest, FormManagerCreatedForOtpForm) {
  FormData form;
  form.set_fields({autofill::test::CreateTestFormField(
      "some_label", "some_name", "some_value",
      autofill::FormControlType::kInputText)});

  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent);
  otp_manager_.ProcessClassificationModelPredictions(
      form, {{form.fields()[0].global_id(), autofill::ONE_TIME_CODE}});

  EXPECT_TRUE(otp_manager_.form_managers().contains(form.global_id()));
  std::vector<autofill::FieldGlobalId> expected_otp_field_ids = {
      form.fields()[0].global_id()};
  EXPECT_EQ(expected_otp_field_ids,
            otp_manager_.form_managers().at(form.global_id()).otp_field_ids());
}

TEST_F(OtpManagerTest, FormManagerNotCreatedForNotFillableForm) {
  FormData form;
  form.set_fields({autofill::test::CreateTestFormField(
      "some_label", "some_name", "some_value",
      // Radio element cannot be filled with a text value, this prediction
      // should not be taken into account.
      autofill::FormControlType::kInputRadio)});

  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent).Times(0);
  otp_manager_.ProcessClassificationModelPredictions(
      form, {{form.fields()[0].global_id(), autofill::ONE_TIME_CODE}});

  EXPECT_FALSE(otp_manager_.form_managers().contains(form.global_id()));
}

TEST_F(OtpManagerTest, ManagersUpdatedWhenPredictionsChange) {
  FormData form;
  form.set_fields({autofill::test::CreateTestFormField(
                       "some_label1", "some_name1", "some_value1",
                       autofill::FormControlType::kInputText),
                   {autofill::test::CreateTestFormField(
                       "some_label2", "some_name2", "some_value2",
                       autofill::FormControlType::kInputPassword)}});

  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent);
  otp_manager_.ProcessClassificationModelPredictions(
      form, {{form.fields()[0].global_id(), autofill::ONE_TIME_CODE},
             {form.fields()[1].global_id(), autofill::UNKNOWN_TYPE}});
  EXPECT_TRUE(otp_manager_.form_managers().contains(form.global_id()));

  // Simulate receiving new predictions.
  // The client should not be notified the second time.
  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent).Times(0);
  otp_manager_.ProcessClassificationModelPredictions(
      form, {{form.fields()[0].global_id(), autofill::UNKNOWN_TYPE},
             {form.fields()[1].global_id(), autofill::ONE_TIME_CODE}});

  EXPECT_TRUE(otp_manager_.form_managers().contains(form.global_id()));
  // Check that the manager reflects the latest predictions.
  std::vector<autofill::FieldGlobalId> expected_otp_field_ids = {
      form.fields()[1].global_id()};
  EXPECT_EQ(expected_otp_field_ids,
            otp_manager_.form_managers().at(form.global_id()).otp_field_ids());
}

TEST_F(OtpManagerTest, FormManagerdDeletedWhenOtpFieldIsNoLongerParsedAsSuch) {
  FormData form;
  form.set_fields({autofill::test::CreateTestFormField(
      "some_label", "some_name", "some_value",
      autofill::FormControlType::kInputText)});

  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent);
  otp_manager_.ProcessClassificationModelPredictions(
      form, {{form.fields()[0].global_id(), autofill::ONE_TIME_CODE}});
  EXPECT_TRUE(otp_manager_.form_managers().contains(form.global_id()));

  otp_manager_.ProcessClassificationModelPredictions(
      form, {{form.fields()[0].global_id(), autofill::UNKNOWN_TYPE}});
  EXPECT_TRUE(otp_manager_.form_managers().empty());
}

}  // namespace password_manager
