// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/field_info_manager.h"
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
    form_id_ = autofill::test::MakeFormGlobalId();
    field_ids_.push_back(autofill::test::MakeFieldGlobalId());

    field_info_manager_ = std::make_unique<FieldInfoManager>(task_runner_);
    ON_CALL(client_, GetFieldInfoManager)
        .WillByDefault(Return(field_info_manager_.get()));
  }

 protected:
  MockPasswordManagerClient client_;
  FormGlobalId form_id_;
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
  OtpFormManager form_manager(form_id_, field_ids_, &client_);

  // Email field was interacted with last, it should be picked as most probable.
  EXPECT_EQ(OtpSource::kEmail, form_manager.otp_source());
}

TEST_F(OtpFormManagerTest, OtpSourceUpdatedWithNewPredictions) {
  const GURL otp_url(kTestOtpUrl);
  EXPECT_CALL(client_, GetLastCommittedURL)
      .WillRepeatedly(ReturnRef(test_otp_url_));
  OtpFormManager form_manager(form_id_, field_ids_, &client_);
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
  OtpFormManager form_manager(form_id_, field_ids_, &client_);
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

#if BUILDFLAG(IS_ANDROID)
TEST_F(OtpFormManagerTest, SmsOtpBackendRetrievedOnManagerCreation) {
  base::test::ScopedFeatureList scoped_feature_list_(
      features::kAndroidSmsOtpFilling);

  EXPECT_CALL(client_, GetLastCommittedURL).WillOnce(ReturnRef(test_otp_url_));
  EXPECT_CALL(client_, GetSmsOtpBackend);
  OtpFormManager form_manager(form_id_, field_ids_, &client_);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace password_manager
