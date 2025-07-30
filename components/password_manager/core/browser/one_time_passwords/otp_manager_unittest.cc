// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/otp_manager.h"

#include "base/test/test_future.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"
#include "components/password_manager/core/browser/one_time_passwords/sms_otp_backend.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"

namespace password_manager {

namespace {

using autofill::FieldGlobalId;
using autofill::FormData;
using autofill::FormGlobalId;
using testing::Return;
using testing::ReturnRef;

constexpr char kTestOtpUrl[] = "https://www.otp-obsessed.com/verification";

FormData CreateTestForm(const autofill::LocalFrameToken& frame_token) {
  FormData form;
  form.set_fields({autofill::test::CreateTestFormField(
      "some_label", "some_name", "some_value",
      autofill::FormControlType::kInputText)});
  return autofill::test::CreateFormDataForFrame(form, frame_token);
}

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;

  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (const, override));
  MOCK_METHOD(void, InformPasswordChangeServiceOfOtpPresent, (), (override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(SmsOtpBackend*, GetSmsOtpBackend, (), (const, override));
#endif  // BUILDFLAG(IS_ANDROID)
};
}  // namespace

class OtpManagerTest : public testing::Test {
 public:
  OtpManagerTest() : otp_manager_(&mock_client_) {}

  void SetUp() override {
    ON_CALL(mock_client_, GetLastCommittedURL)
        .WillByDefault(ReturnRef(test_otp_url_));
  }

 protected:
  MockPasswordManagerClient mock_client_;
  OtpManager otp_manager_;

 private:
  GURL test_otp_url_ = GURL(kTestOtpUrl);
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
            otp_manager_.form_managers().at(form.global_id())->otp_field_ids());
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
            otp_manager_.form_managers().at(form.global_id())->otp_field_ids());
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

TEST_F(OtpManagerTest, FormManagerCreatedForOtpFormWithServerOverrides) {
  FormData form;
  form.set_fields({autofill::test::CreateTestFormField(
      "some_label", "some_name", "some_value",
      autofill::FormControlType::kInputText)});

  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent);
  otp_manager_.ProcessServerPredictions(
      form, CreateServerPredictions(form, {{0, autofill::ONE_TIME_CODE}},
                                    /*is_override=*/true));

  EXPECT_TRUE(otp_manager_.form_managers().contains(form.global_id()));
  std::vector<autofill::FieldGlobalId> expected_otp_field_ids = {
      form.fields()[0].global_id()};
  EXPECT_EQ(expected_otp_field_ids,
            otp_manager_.form_managers().at(form.global_id())->otp_field_ids());
}

TEST_F(OtpManagerTest, FormManagerDeletedForOtpFormWithNonOtpServerOverrides) {
  FormData form;
  form.set_fields({autofill::test::CreateTestFormField(
      "some_label", "some_name", "some_value",
      autofill::FormControlType::kInputText)});

  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent);
  otp_manager_.ProcessClassificationModelPredictions(
      form, {{form.fields()[0].global_id(), autofill::ONE_TIME_CODE}});

  otp_manager_.ProcessServerPredictions(
      form, CreateServerPredictions(form, {{0, autofill::PASSWORD}},
                                    /*is_override=*/true));

  EXPECT_FALSE(otp_manager_.form_managers().contains(form.global_id()));
}

TEST_F(OtpManagerTest, FormManagerUpdatedWithServerOverrides) {
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

  otp_manager_.ProcessServerPredictions(
      form,
      CreateServerPredictions(
          form, {{0, autofill::UNKNOWN_TYPE}, {1, autofill::ONE_TIME_CODE}},
          /*is_override=*/true));

  EXPECT_TRUE(otp_manager_.form_managers().contains(form.global_id()));
  std::vector<autofill::FieldGlobalId> expected_otp_field_ids = {
      form.fields()[1].global_id()};
  EXPECT_EQ(expected_otp_field_ids,
            otp_manager_.form_managers().at(form.global_id())->otp_field_ids());
}

TEST_F(OtpManagerTest, FormManagerNotUpdatedWithNotOverridePredictions) {
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

  otp_manager_.ProcessServerPredictions(
      form,
      CreateServerPredictions(
          form, {{0, autofill::UNKNOWN_TYPE}, {1, autofill::ONE_TIME_CODE}},
          /*is_override=*/false));

  EXPECT_TRUE(otp_manager_.form_managers().contains(form.global_id()));
  std::vector<autofill::FieldGlobalId> expected_otp_field_ids = {
      form.fields()[0].global_id()};
  EXPECT_EQ(expected_otp_field_ids,
            otp_manager_.form_managers().at(form.global_id())->otp_field_ids());
}

TEST_F(OtpManagerTest, CleanFormManagersCacheForIndividualFrames) {
  autofill::LocalFrameToken frame_token1 =
      autofill::test::MakeLocalFrameToken();
  FormData form1 = CreateTestForm(frame_token1);

  autofill::LocalFrameToken frame_token2 =
      autofill::test::MakeLocalFrameToken();
  FormData form2 = CreateTestForm(frame_token2);

  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent).Times(2);
  otp_manager_.ProcessClassificationModelPredictions(
      form1, {{form1.fields()[0].global_id(), autofill::ONE_TIME_CODE}});
  otp_manager_.ProcessClassificationModelPredictions(
      form2, {{form2.fields()[0].global_id(), autofill::ONE_TIME_CODE}});

  ASSERT_EQ(2u, otp_manager_.form_managers().size());

  // Simulate the first frame being deleted and verify that the form manager
  // managing the form in the first frame is deleted too.
  otp_manager_.OnRenderFrameDeleted(frame_token1);
  EXPECT_EQ(1u, otp_manager_.form_managers().size());
  EXPECT_FALSE(otp_manager_.form_managers().contains(form1.global_id()));
  EXPECT_TRUE(otp_manager_.form_managers().contains(form2.global_id()));

  // Simulate the second frame navigating and verify that the form manager
  // managing the form in the second frame is deleted.
  otp_manager_.OnDidFinishNavigationInIframe(frame_token2);
  EXPECT_EQ(0u, otp_manager_.form_managers().size());
}

TEST_F(OtpManagerTest, CleanFormManagersCacheOnMainFrameNavigation) {
  autofill::LocalFrameToken frame_token1 =
      autofill::test::MakeLocalFrameToken();
  FormData form1 = CreateTestForm(frame_token1);

  autofill::LocalFrameToken frame_token2 =
      autofill::test::MakeLocalFrameToken();
  FormData form2 = CreateTestForm(frame_token2);

  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent).Times(2);
  otp_manager_.ProcessClassificationModelPredictions(
      form1, {{form1.fields()[0].global_id(), autofill::ONE_TIME_CODE}});
  otp_manager_.ProcessClassificationModelPredictions(
      form2, {{form2.fields()[0].global_id(), autofill::ONE_TIME_CODE}});

  ASSERT_EQ(2u, otp_manager_.form_managers().size());

  otp_manager_.OnDidFinishNavigationInMainFrame();
  EXPECT_EQ(0u, otp_manager_.form_managers().size());
}

#if BUILDFLAG(IS_ANDROID)
class MockSmsOtpBackend : public SmsOtpBackend {
 public:
  MockSmsOtpBackend() = default;

  MOCK_METHOD(void,
              RetrieveSmsOtp,
              (base::OnceCallback<void(const OtpFetchReply&)>),
              (override));
};

class OtpManagerTestWithSmsBackend : public OtpManagerTest {
 public:
  void SetUp() override {
    OtpManagerTest::SetUp();
    ON_CALL(mock_client_, GetSmsOtpBackend)
        .WillByDefault(Return(&sms_otp_backend_));
  }

 protected:
  MockSmsOtpBackend sms_otp_backend_;

 private:
  base::test::ScopedFeatureList feature_list_{features::kAndroidSmsOtpFilling};
};

TEST_F(OtpManagerTestWithSmsBackend, OtpFillingWithOtpValueRetrieved) {
  // Simulate OTP backend providing a value.
  std::string otp_value = "123456";
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(testing::Invoke(
          [&otp_value](
              base::OnceCallback<void(const OtpFetchReply&)> callback) {
            std::move(callback).Run(OtpFetchReply(otp_value,
                                                  /*request_complete=*/true));
          }));

  FormData form;
  form.set_fields({autofill::test::CreateTestFormField(
      "some_label", "some_name", "some_value",
      autofill::FormControlType::kInputText)});
  EXPECT_CALL(mock_client_, InformPasswordChangeServiceOfOtpPresent);
  otp_manager_.ProcessClassificationModelPredictions(
      form, {{form.fields()[0].global_id(), autofill::ONE_TIME_CODE}});

  // A field parsed as an OTP field is eligible for OTP filling.
  EXPECT_TRUE(otp_manager_.IsFieldEligibleForOtpFilling(
      form.global_id(), form.fields()[0].global_id()));
  base::test::TestFuture<std::vector<std::string>> completion_future;
  otp_manager_.GetOtpSuggestions(form.global_id(), form.fields()[0].global_id(),
                                 completion_future.GetCallback());
  ASSERT_TRUE(completion_future.Wait());
  std::vector<std::string> expected_otp_values = {otp_value};
  std::vector<std::string> received_otp_values = completion_future.Take();
  EXPECT_EQ(expected_otp_values, received_otp_values);

  FormGlobalId unrelated_form_id = autofill::test::MakeFormGlobalId();
  EXPECT_FALSE(otp_manager_.IsFieldEligibleForOtpFilling(
      unrelated_form_id, form.fields()[0].global_id()));
  FieldGlobalId unrelated_field_id = autofill::test::MakeFieldGlobalId();
  EXPECT_FALSE(otp_manager_.IsFieldEligibleForOtpFilling(form.global_id(),
                                                         unrelated_field_id));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
