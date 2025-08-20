// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/one_time_passwords/sms_otp_backend.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"

namespace password_manager {

namespace {

using autofill::FieldGlobalId;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormGlobalId;
using testing::Return;
using testing::ReturnRef;

constexpr char kTestSignonRealm[] = "https://www.otp-obsessed.com";
constexpr char kTestOtpUrl[] = "https://www.otp-obsessed.com/verification";

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;

  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (const, override));
  MOCK_METHOD(FieldInfoManager*, GetFieldInfoManager, (), (const, override));
#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(SmsOtpBackend*, GetSmsOtpBackend, (), (const, override));
#endif  // BUILDFLAG(IS_ANDROID)
};

FieldInfo CreatePhoneNumberFieldInfo() {
  return FieldInfo(/*driver_id=*/1, FieldRendererId(100), kTestSignonRealm,
                   u"911-112-01",
                   /*is_likely_otp*/ false);
}

FieldInfo CreateEmailAddressFieldInfo() {
  return FieldInfo(/*driver_id=*/1, FieldRendererId(200), kTestSignonRealm,
                   u"user@gmail.com",
                   /*is_likely_otp*/ false);
}

}  // namespace

class OtpFormManagerTest : public testing::Test {
 public:
  OtpFormManagerTest() : task_runner_(new base::TestMockTimeTaskRunner) {}

  void SetUp() override {
    form_data_.set_fields({autofill::test::CreateTestFormField(
        "some_label", "some_name", "some_value",
        autofill::FormControlType::kInputText)});
    field_ids_.push_back(autofill::test::MakeFieldGlobalId());

    field_info_manager_ = std::make_unique<FieldInfoManager>(task_runner_);
    ON_CALL(client_, GetFieldInfoManager)
        .WillByDefault(Return(field_info_manager_.get()));
    ON_CALL(client_, GetLastCommittedURL)
        .WillByDefault(ReturnRef(test_otp_url_));
  }

 protected:
  MockPasswordManagerClient client_;
  FormData form_data_;
  std::vector<FieldGlobalId> field_ids_;
  std::unique_ptr<FieldInfoManager> field_info_manager_;
  GURL test_otp_url_ = GURL(kTestOtpUrl);

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

TEST_F(OtpFormManagerTest, BasicOtpSourceIdentification) {
  // Simulate user interacting with phone number and email fields.
  field_info_manager_->AddFieldInfo(CreatePhoneNumberFieldInfo(),
                                    FormPredictions());
  field_info_manager_->AddFieldInfo(CreateEmailAddressFieldInfo(),
                                    FormPredictions());

  const GURL otp_url(kTestOtpUrl);
  EXPECT_CALL(client_, GetLastCommittedURL).WillOnce(ReturnRef(test_otp_url_));
  OtpFormManager form_manager(form_data_, field_ids_, &client_);

  // Email field was interacted with last, it should be picked as most probable.
  EXPECT_EQ(OtpSource::kEmail, form_manager.otp_source());
}

TEST_F(OtpFormManagerTest, OtpSourceUpdatedWithNewPredictions) {
  const GURL otp_url(kTestOtpUrl);
  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillRepeatedly(ReturnRef(test_otp_url_));
  OtpFormManager form_manager(form_data_, field_ids_, &client_);
  EXPECT_EQ(OtpSource::kUnknown, form_manager.otp_source());

  // Simulate user interacting with a phone number field.
  field_info_manager_->AddFieldInfo(CreatePhoneNumberFieldInfo(),
                                    FormPredictions());

  // Simulate form changing dynamically, which triggers receiving new
  // predictions.
  std::vector<FieldGlobalId> new_field_ids = {
      autofill::test::MakeFieldGlobalId()};
  form_manager.ProcessUpdatedPredictions(new_field_ids);

  // Check that the new OTP source is identified.
  EXPECT_EQ(OtpSource::kSms, form_manager.otp_source());
}

TEST_F(OtpFormManagerTest, OtpSourceNotRemovedOnceDataGetsStale) {
  // Simulate user interacting with a phone number field.
  field_info_manager_->AddFieldInfo(CreatePhoneNumberFieldInfo(),
                                    FormPredictions());

  const GURL otp_url(kTestOtpUrl);
  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillRepeatedly(ReturnRef(test_otp_url_));
  OtpFormManager form_manager(form_data_, field_ids_, &client_);
  EXPECT_EQ(OtpSource::kSms, form_manager.otp_source());

  // To keep the test simple, reset the FieldInfoManager, so no data is
  // available.
  field_info_manager_.reset();
  ON_CALL(client_, GetFieldInfoManager).WillByDefault(Return(nullptr));

  // Simulate form changing dynamically, which triggers receiving new
  // predictions.
  std::vector<FieldGlobalId> new_field_ids = {
      autofill::test::MakeFieldGlobalId()};
  form_manager.ProcessUpdatedPredictions(new_field_ids);

  // Check that the OTP source is still available.
  EXPECT_EQ(OtpSource::kSms, form_manager.otp_source());
}

TEST_F(OtpFormManagerTest, GetFillDataForOtpSuggestion) {
  FieldGlobalId field_id1 = autofill::test::MakeFieldGlobalId();
  FieldGlobalId field_id2 = autofill::test::MakeFieldGlobalId();
  FieldGlobalId field_id3 = autofill::test::MakeFieldGlobalId();
  FieldGlobalId field_id4 = autofill::test::MakeFieldGlobalId();
  std::vector<FieldGlobalId> otp_fields = {field_id1, field_id2, field_id3,
                                           field_id4};
  OtpFormManager form_manager(form_data_, otp_fields, &client_);

  // Case 1: OTP value is longer than the number of detected fields.
  autofill::OtpFillData fill_data =
      form_manager.GetFillDataForOtpSuggestion(field_id1, u"12345");
  EXPECT_EQ(1u, fill_data.size());
  EXPECT_EQ(u"12345", fill_data.at(field_id1));

  // Case 2: OTP value matches the count of deceted otp fields.
  fill_data = form_manager.GetFillDataForOtpSuggestion(field_id1, u"1234");
  EXPECT_EQ(4u, fill_data.size());
  EXPECT_EQ(u"1", fill_data.at(field_id1));
  EXPECT_EQ(u"2", fill_data.at(field_id2));
  EXPECT_EQ(u"3", fill_data.at(field_id3));
  EXPECT_EQ(u"4", fill_data.at(field_id4));

  // Case 3: OTP value is shorter than the number of detected fields and there
  // are enough fields to fill the value starting from the middle.
  fill_data = form_manager.GetFillDataForOtpSuggestion(field_id1, u"12");
  EXPECT_EQ(2u, fill_data.size());
  EXPECT_EQ(u"1", fill_data.at(field_id1));
  EXPECT_EQ(u"2", fill_data.at(field_id2));

  // Case 4: OTP value is shorter than the number of detected fields and there
  // are NOT enough fields to fill the value starting from the middle.
  fill_data = form_manager.GetFillDataForOtpSuggestion(field_id4, u"12");
  EXPECT_EQ(1u, fill_data.size());
  EXPECT_EQ(u"12", fill_data.at(field_id4));

  // Case 5: OTP field is not among currently detected OTP fields.
  FieldGlobalId field_id5 = autofill::test::MakeFieldGlobalId();
  fill_data = form_manager.GetFillDataForOtpSuggestion(field_id5, u"1234");
  EXPECT_EQ(1u, fill_data.size());
  EXPECT_EQ(u"1234", fill_data.at(field_id5));
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

class OtpFormManagerTestWithSmsBackend : public OtpFormManagerTest {
 public:
  void SetUp() override {
    OtpFormManagerTest::SetUp();
    ON_CALL(client_, GetSmsOtpBackend).WillByDefault(Return(&sms_otp_backend_));
  }

 protected:
  MockSmsOtpBackend sms_otp_backend_;

 private:
  base::test::ScopedFeatureList feature_list_{features::kAndroidSmsOtpFilling};
};

TEST_F(OtpFormManagerTestWithSmsBackend, SmsOtpBackendUsedForSmsOtpRetrieval) {
  // Check that the backend is called on the form manager creation.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp);
  OtpFormManager form_manager(form_data_, field_ids_, &client_);

  // Simulate form changing dynamically, which should result in receiving new
  // predictions and trigger the new backend query.
  field_info_manager_->AddFieldInfo(CreatePhoneNumberFieldInfo(),
                                    FormPredictions());
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp);
  form_manager.ProcessUpdatedPredictions({autofill::test::MakeFieldGlobalId()});
}

TEST_F(OtpFormManagerTestWithSmsBackend, OtpFillingWithOtpValueRetrieved) {
  std::string otp_value = "123456";
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(testing::Invoke(
          [&otp_value](
              base::OnceCallback<void(const OtpFetchReply&)> callback) {
            std::move(callback).Run(OtpFetchReply(otp_value,
                                                  /*request_complete=*/true));
          }));
  FieldGlobalId otp_field_id = autofill::test::MakeFieldGlobalId();
  OtpFormManager form_manager(form_data_, {otp_field_id}, &client_);

  // A field not parsed as an OTP field is not eligible for OTP filling.
  FieldGlobalId some_other_field_id = autofill::test::MakeFieldGlobalId();
  EXPECT_FALSE(form_manager.IsFieldEligibleForOtpFilling(some_other_field_id));

  // A field parsed as an OTP field is eligible for OTP filling.
  EXPECT_TRUE(form_manager.IsFieldEligibleForOtpFilling(otp_field_id));
  base::test::TestFuture<std::vector<std::string>> completion_future;
  form_manager.GetOtpSuggestions(otp_field_id, completion_future.GetCallback());
  ASSERT_TRUE(completion_future.Wait());
  std::vector<std::string> expected_otp_values = {otp_value};
  std::vector<std::string> received_otp_values = completion_future.Take();
  EXPECT_EQ(expected_otp_values, received_otp_values);
}

TEST_F(OtpFormManagerTestWithSmsBackend, OtpFillingEligibilityOtpValueMissing) {
  // Simulate OTP retieval request not providing a value.
  EXPECT_CALL(sms_otp_backend_, RetrieveSmsOtp)
      .WillOnce(testing::Invoke(
          [](base::OnceCallback<void(const OtpFetchReply&)> callback) {
            std::move(callback).Run(OtpFetchReply(/*otp_value=*/std::nullopt,
                                                  /*request_complete=*/true));
          }));
  FieldGlobalId otp_field_id = autofill::test::MakeFieldGlobalId();
  OtpFormManager form_manager(form_data_, {otp_field_id}, &client_);

  FieldGlobalId some_other_field_id = autofill::test::MakeFieldGlobalId();
  EXPECT_FALSE(form_manager.IsFieldEligibleForOtpFilling(some_other_field_id));
  EXPECT_FALSE(form_manager.IsFieldEligibleForOtpFilling(otp_field_id));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
