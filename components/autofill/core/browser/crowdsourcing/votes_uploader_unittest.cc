// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/language_code.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::Return;
using testing::Truly;

namespace autofill {

using one_time_tokens::OneTimeToken;
using one_time_tokens::OneTimeTokenType;

namespace {

class MockOneTimeTokenService : public one_time_tokens::OneTimeTokenService {
 public:
  MOCK_METHOD(void, GetRecentOneTimeTokens, (Callback callback), (override));
  MOCK_METHOD(std::vector<OneTimeToken>,
              GetCachedOneTimeTokens,
              (),
              (const override));
  MOCK_METHOD(one_time_tokens::ExpiringSubscription,
              Subscribe,
              (base::Time expiration, Callback callback),
              (override));
};

class AutofillVotesUploaderTest : public testing::Test,
                                  public WithTestAutofillClientDriverManager<> {
 public:
  AutofillVotesUploaderTest() = default;
  ~AutofillVotesUploaderTest() override = default;

  void SetUp() override {
    InitAutofillClient();
    autofill_client().SetPrefs(test::PrefServiceForTesting());
    AddTestProfile();
    feature_list_.InitAndEnableFeature(features::kAutofillSmsOtpCrowdsourcing);

    std::unique_ptr<MockOneTimeTokenService> mock_service =
        std::make_unique<MockOneTimeTokenService>();
    autofill_client().set_one_time_token_service(std::move(mock_service));
  }

 protected:
  using enum AutofillDriver::LifecycleState;

  std::unique_ptr<FormStructure> CreateTestForm() {
    return std::make_unique<FormStructure>(test::CreateTestAddressFormData());
  }

  std::unique_ptr<FormStructure> CreateCreditCardForm() {
    return std::make_unique<FormStructure>(test::CreateTestCreditCardFormData(
        /*is_https=*/true, /*use_month_type=*/false));
  }

  std::unique_ptr<FormStructure> CreateOtpForm() {
    return std::make_unique<FormStructure>(test::CreateTestOtpFormData());
  }

  void AddTestProfile() {
    autofill_client()
        .GetPersonalDataManager()
        .address_data_manager()
        .AddProfile(test::GetFullProfile());
  }

  void AddTestCreditCard() {
    autofill_client()
        .GetPersonalDataManager()
        .payments_data_manager()
        .AddCreditCard(test::GetCreditCard());
  }

  MockAutofillCrowdsourcingManager& GetCrowdsourcingManager() {
    return static_cast<MockAutofillCrowdsourcingManager&>(
        autofill_client().GetCrowdsourcingManager());
  }

  MockOneTimeTokenService& GetMockOneTimeTokenService() {
    return *static_cast<MockOneTimeTokenService*>(
        autofill_client().GetOneTimeTokenService());
  }

  auto QuitRunLoopAndReturnTrue(base::RunLoop& run_loop) {
    return [&run_loop](auto&&...) {
      run_loop.Quit();
      return true;
    };
  }

  void TriggerFrameStateChange(TestAutofillDriver& driver,
                               AutofillDriver::LifecycleState from,
                               AutofillDriver::LifecycleState to) {
    static_cast<AutofillDriverFactory::Observer&>(
        autofill_client().GetVotesUploader())
        .OnAutofillDriverStateChanged(
            autofill_client().GetAutofillDriverFactory(), driver, from, to);
  }

  std::unique_ptr<FormStructure> CreateTestFormWithFrame(
      const TestAutofillDriver& driver) {
    FormData form_data = test::CreateTestAddressFormData();
    form_data.set_host_frame(driver.GetFrameToken());
    return std::make_unique<FormStructure>(form_data);
  }

  bool MaybeStartVoteUploadProcess(std::unique_ptr<FormStructure> form,
                                   bool observed_submission) {
    return autofill_client().GetVotesUploader().MaybeStartVoteUploadProcess(
        std::move(form), observed_submission, LanguageCode("en"),
        base::TimeTicks::Now(), u"", ukm::kInvalidSourceId);
  }

  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

// Test basic vote upload process for form submission.
TEST_F(AutofillVotesUploaderTest, BasicVoteUpload) {
  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));

  EXPECT_TRUE(MaybeStartVoteUploadProcess(CreateTestForm(),
                                          /*observed_submission=*/true));
  run_loop.Run();
}

// Test that vote upload returns false when no local profile data is available.
TEST_F(AutofillVotesUploaderTest, NoLocalProfileReturnsFalse) {
  autofill_client()
      .GetPersonalDataManager()
      .address_data_manager()
      .RemoveLocalProfilesModifiedBetween(base::Time::Min(), base::Time::Max());

  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest).Times(0);
  EXPECT_FALSE(MaybeStartVoteUploadProcess(CreateTestForm(),
                                           /*observed_submission=*/true));
  task_environment_.RunUntilIdle();
}

// Test that pending votes don't trigger immediate upload while submission votes
// do.
TEST_F(AutofillVotesUploaderTest, PendingVotesVsSubmissionVotes) {
  // Test pending vote (should not trigger immediate upload).
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest).Times(0);
  EXPECT_TRUE(MaybeStartVoteUploadProcess(CreateTestForm(),
                                          /*observed_submission=*/false));
  task_environment_.RunUntilIdle();

  // Test submission vote (should trigger immediate upload).
  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));

  EXPECT_TRUE(MaybeStartVoteUploadProcess(CreateTestForm(),
                                          /*observed_submission=*/true));
  run_loop.Run();
}

// Test vote upload for credit card forms.
TEST_F(AutofillVotesUploaderTest, CreditCardFormUpload) {
  AddTestCreditCard();

  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));

  EXPECT_TRUE(MaybeStartVoteUploadProcess(CreateCreditCardForm(),
                                          /*observed_submission=*/true));
  run_loop.Run();
}

// Test vote upload for OTP forms.
TEST_F(AutofillVotesUploaderTest, OtpFormUpload) {
  constexpr char kOtp[] = "123456";
  EXPECT_CALL(GetMockOneTimeTokenService(), GetCachedOneTimeTokens())
      .WillOnce(Return(std::vector<OneTimeToken>{
          OneTimeToken(OneTimeTokenType::kSmsOtp, kOtp, base::Time::Now())}));

  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));

  EXPECT_TRUE(MaybeStartVoteUploadProcess(CreateOtpForm(),
                                          /*observed_submission=*/true));
  run_loop.Run();
}

// Test that votes for distinct forms are uploaded separately.
TEST_F(AutofillVotesUploaderTest, DistinctFormsUploadedSeparately) {
  std::unique_ptr<FormStructure> form1 = CreateTestForm();
  std::unique_ptr<FormStructure> form2 = CreateCreditCardForm();
  ASSERT_NE(form1->form_signature(), form2->form_signature());

  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));

  // Enqueue votes for two different forms.
  EXPECT_TRUE(MaybeStartVoteUploadProcess(std::move(form1),
                                          /*observed_submission=*/true));
  EXPECT_TRUE(MaybeStartVoteUploadProcess(std::move(form2),
                                          /*observed_submission=*/true));
  run_loop.Run();
}

// Test that only the most recent vote for the same form is uploaded.
TEST_F(AutofillVotesUploaderTest, MostRecentVoteForSameFormWins) {
  CreateAutofillDriver();
  FormData form_data = test::CreateTestAddressFormData();
  form_data.set_host_frame(autofill_driver().GetFrameToken());

  auto form1 = std::make_unique<FormStructure>(form_data);
  auto form2 = std::make_unique<FormStructure>(form_data);
  form2->fields()[0]->set_value(
      test::GetFullProfile().GetInfo(NAME_FULL, "en"));

  ASSERT_EQ(form1->form_signature(), form2->form_signature());
  ASSERT_TRUE(form1->fields()[0]->value().empty());
  ASSERT_FALSE(form2->fields()[0]->value().empty());

  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(),
              StartUploadRequest(
                  ElementsAre(Truly([](const AutofillUploadContents& upload) {
                    return upload.field_data_size() >= 1 &&
                           upload.field_data(0).autofill_type_size() == 1 &&
                           upload.field_data(0).autofill_type(0) == NAME_FULL;
                  })),
                  _, _))
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));

  // Enqueue first pending vote for a form.
  EXPECT_TRUE(MaybeStartVoteUploadProcess(std::move(form1),
                                          /*observed_submission=*/false));
  // Enqueue second pending vote for the same form with field value set.
  EXPECT_TRUE(MaybeStartVoteUploadProcess(std::move(form2),
                                          /*observed_submission=*/false));

  // Flush the votes.
  TriggerFrameStateChange(autofill_driver(), kActive, kInactive);
  run_loop.Run();
}

// Test that pending votes don't trigger immediate upload.
TEST_F(AutofillVotesUploaderTest, PendingVotesNoImmediateUpload) {
  // Test that pending votes are stored without immediate upload.
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest).Times(0);

  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(MaybeStartVoteUploadProcess(CreateTestForm(),
                                            /*observed_submission=*/false));
  }
  task_environment_.RunUntilIdle();
}

// Test that pending votes are flushed when a submission happens.
TEST_F(AutofillVotesUploaderTest, PendingVotesFlushedOnSubmission) {
  // Store some pending votes.
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest).Times(0);

  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(MaybeStartVoteUploadProcess(CreateTestForm(),
                                            /*observed_submission=*/false));
  }
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));
  // A submission should trigger upload of pending votes.
  MaybeStartVoteUploadProcess(CreateTestForm(),
                              /*observed_submission=*/true);
  run_loop.Run();
}

// Test that pending votes are flushed when frame becomes inactive.
TEST_F(AutofillVotesUploaderTest, PendingVotesFlushedOnFrameInactive) {
  CreateAutofillDriver();
  // Store a pending vote for a specific frame token that matches our test
  // driver.
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest).Times(0);
  // Create form and associate it with the test driver's frame.
  EXPECT_TRUE(
      MaybeStartVoteUploadProcess(CreateTestFormWithFrame(autofill_driver()),
                                  /*observed_submission=*/false));
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));
  // Trigger frame inactive state change - this should flush pending votes.
  TriggerFrameStateChange(autofill_driver(), kActive, kInactive);
  run_loop.Run();
}

// Test that pending votes are flushed when frame is reset.
TEST_F(AutofillVotesUploaderTest, PendingVotesFlushedOnFrameReset) {
  CreateAutofillDriver();
  // Store a pending vote.
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest).Times(0);
  EXPECT_TRUE(
      MaybeStartVoteUploadProcess(CreateTestFormWithFrame(autofill_driver()),
                                  /*observed_submission=*/false));
  task_environment_.RunUntilIdle();

  // First transition to PendingReset should not flush votes.
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest).Times(0);
  TriggerFrameStateChange(autofill_driver(), kActive, kPendingReset);
  task_environment_.RunUntilIdle();

  // Then transition out of PendingReset - this should flush pending votes.
  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));
  TriggerFrameStateChange(autofill_driver(), kPendingReset, kActive);
  run_loop.Run();
}

// Test that pending votes are flushed when frame is deleted.
TEST_F(AutofillVotesUploaderTest, PendingVotesFlushedOnFrameDeletion) {
  CreateAutofillDriver();
  // Store a pending vote.
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest).Times(0);
  EXPECT_TRUE(
      MaybeStartVoteUploadProcess(CreateTestFormWithFrame(autofill_driver()),
                                  /*observed_submission=*/false));
  task_environment_.RunUntilIdle();

  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));
  // Trigger frame deletion - this should flush pending votes.
  TriggerFrameStateChange(autofill_driver(), kActive, kPendingDeletion);
  run_loop.Run();
}

// Test that multiple pending votes can be added and queue functionality works.
TEST_F(AutofillVotesUploaderTest, PendingVotesQueueFunctionality) {
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest).Times(0);

  // Add several pending votes to verify they are queued properly.
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(MaybeStartVoteUploadProcess(CreateTestForm(),
                                            /*observed_submission=*/false));
  }
  task_environment_.RunUntilIdle();

  // Verify that a submission still triggers upload.
  base::RunLoop run_loop;
  EXPECT_CALL(GetCrowdsourcingManager(), StartUploadRequest)
      .WillOnce(QuitRunLoopAndReturnTrue(run_loop));

  MaybeStartVoteUploadProcess(CreateTestForm(),
                              /*observed_submission=*/true);
  run_loop.Run();
}

}  // namespace
}  // namespace autofill
