// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_coordinator.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/form_cache_query_util.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_service.h"
#include "components/one_time_tokens/core/browser/fake_sms_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

class OtpCoordinatorTestApi {
 public:
  explicit OtpCoordinatorTestApi(OtpCoordinator& coordinator)
      : coordinator_(coordinator) {}

  OtpTargetFieldIdentifier& target_field_identifier() {
    return coordinator_->target_field_identifier_;
  }
  const base::flat_map<FieldGlobalId, OtpDelivery>& deliveries() const {
    return coordinator_->deliveries_;
  }

 private:
  const raw_ref<OtpCoordinator> coordinator_;
};

namespace {

using ::autofill::test::FormDescription;
using ::autofill::test::GetServerTypes;
using ::one_time_tokens::OneTimeToken;
using ::one_time_tokens::OneTimeTokenRetrievalError;
using ::one_time_tokens::OneTimeTokenType;
using ::testing::Test;

constexpr char kTestUrl[] = "https://example.test";

class TestFrameObserver : public OtpCoordinator::Observer {
 public:
  // `manager` and `coordinator` must be non-null and outlive this observer.
  TestFrameObserver(BrowserAutofillManager* manager,
                    OtpCoordinator* coordinator)
      : manager_(CHECK_DEREF(manager)), coordinator_(CHECK_DEREF(coordinator)) {
    coordinator_->RegisterObserver(this);
  }
  ~TestFrameObserver() override { coordinator_->UnregisterObserver(this); }

  // OtpCoordinator::Observer:
  BrowserAutofillManager& GetBrowserAutofillManager() const override {
    return *manager_;
  }
  void OnOtpAvailable(const OtpSuggestionData& data) override {
    ++otp_available_notifications_;
    if (on_otp_available_) {
      std::move(on_otp_available_).Run();
    }
  }

  void WaitForNotifications(size_t expected_count) {
    while (otp_available_notifications_ < expected_count) {
      base::test::TestFuture<void> future;
      on_otp_available_ = future.GetCallback();
      ASSERT_TRUE(future.Wait());
    }
    EXPECT_EQ(otp_available_notifications_, expected_count);
  }

  size_t otp_available_notifications() const {
    return otp_available_notifications_;
  }

 private:
  const raw_ref<BrowserAutofillManager> manager_;
  const raw_ref<OtpCoordinator> coordinator_;
  size_t otp_available_notifications_ = 0;
  base::OnceClosure on_otp_available_;
};

class OtpCoordinatorTest
    : public Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 TestAutofillDriver> {
 protected:
  OtpCoordinatorTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillShowGmailOtpSuggestions);
  }

  void SetUp() override {
    InitAutofillClient();
    prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(),
                                             true);
    autofill_client().set_last_committed_primary_main_frame_url(GURL(kTestUrl));

    auto sms_service = std::make_unique<one_time_tokens::FakeSmsOtpService>();
    sms_otp_service_ = sms_service.get();
    autofill_client().set_sms_otp_service(std::move(sms_service));

    auto gmail_service =
        std::make_unique<one_time_tokens::FakeGmailOtpService>();
    gmail_service_ = gmail_service.get();
    autofill_client().set_gmail_otp_service(std::move(gmail_service));

    CreateAutofillDriver();
    coordinator_ = std::make_unique<OtpCoordinator>(&autofill_client());
    frame_observer_ = std::make_unique<TestFrameObserver>(&autofill_manager(),
                                                          coordinator_.get());
  }

  void TearDown() override {
    frame_observer_.reset();
    coordinator_.reset();
    sms_otp_service_ = nullptr;
    gmail_service_ = nullptr;
    DestroyAutofillClient();
  }

  OtpCoordinatorTestApi test_api_coordinator() const {
    return OtpCoordinatorTestApi(*coordinator_);
  }

  std::vector<OtpTarget> AddOtpForm(size_t fields = 1,
                                    GURL url = GURL(kTestUrl),
                                    bool listen = true) {
    FormDescription form_description = {
        .host_frame = autofill_driver().GetFrameToken(),
        .main_frame_origin = url::Origin::Create(url),
    };
    for (size_t i = 0; i < fields; ++i) {
      form_description.fields.push_back(
          {.server_type = ONE_TIME_CODE,
           .host_frame = autofill_driver().GetFrameToken(),
           .origin = url::Origin::Create(url)});
    }
    FormData form = test::GetFormData(form_description);
    FormGlobalId form_id = form.global_id();
    auto form_structure = std::make_unique<FormStructure>(form);
    test_api(*form_structure).SetFieldTypes(GetServerTypes(form_description));
    test_api(*form_structure).AssignSections();
    test_api(autofill_manager())
        .AddSeenFormStructure(std::move(form_structure));

    const FormStructure* cached_form =
        autofill_manager().FindCachedFormById(form_id);
    std::vector<OtpTarget> targets;
    for (size_t i = 0; i < fields; ++i) {
      const OtpTarget target = BuildOtpTarget(autofill_manager(), *cached_form,
                                              *cached_form->field(i));
      if (listen) {
        coordinator_->StartListeningForOtp(target);
      }
      targets.push_back(target);
    }
    return targets;
  }

  void SendGmailOtp(const std::string& code,
                    base::Time email_time,
                    const std::string& sender = "noreply@example.test") {
    task_environment_.AdvanceClock(base::Milliseconds(1));
    gmail_service_->ProcessCallbacks(OneTimeToken(
        OneTimeTokenType::kGmail, code, base::TimeTicks::Now(), sender,
        email_time));
  }

  std::string DeliveredTokenValue(const OtpTarget& target) const {
    return test_api_coordinator()
        .deliveries()
        .at(target.field)
        .GetAvailableCandidate()
        ->token.value();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  raw_ptr<one_time_tokens::FakeSmsOtpService> sms_otp_service_ = nullptr;
  raw_ptr<one_time_tokens::FakeGmailOtpService> gmail_service_ = nullptr;
  std::unique_ptr<OtpCoordinator> coordinator_;
  std::unique_ptr<TestFrameObserver> frame_observer_;
};

TEST_F(OtpCoordinatorTest, IgnoresIneligibleTargets) {
  OtpTarget ineligible_target(
      url::Origin::Create(GURL(kTestUrl)), autofill_driver().GetFrameToken(),
      FormGlobalId(), FieldGlobalId(), autofill_driver().GetFrameToken());
  coordinator_->StartListeningForOtp(ineligible_target);
  EXPECT_TRUE(test_api_coordinator().deliveries().empty());
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 0u);
}

TEST_F(OtpCoordinatorTest, UnregisteringOneObserverKeepsSharedManagerEligible) {
  auto second_observer = std::make_unique<TestFrameObserver>(
      &autofill_manager(), coordinator_.get());
  frame_observer_.reset();

  const OtpTarget target = AddOtpForm()[0];
  EXPECT_EQ(test_api_coordinator().deliveries().size(), 1u);
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 1u);
}

TEST_F(OtpCoordinatorTest,
       RenewsSmsSubscriptionAfterErrorAndPreservesTypingState) {
  const OtpTarget target = AddOtpForm()[0];
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 1u);

  test_api_coordinator().target_field_identifier().OnTextFieldValueChanged();
  EXPECT_TRUE(test_api_coordinator()
                  .target_field_identifier()
                  .TypedInAnyFieldAfterOtpDetection());

  sms_otp_service_->SimulateOtpArrived(
      base::unexpected(OneTimeTokenRetrievalError::kSmsOtpBackendTimeout));
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 0u);

  coordinator_->StartListeningForOtp(target);
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 1u);
  EXPECT_TRUE(test_api_coordinator()
                  .target_field_identifier()
                  .TypedInAnyFieldAfterOtpDetection());
}

TEST_F(OtpCoordinatorTest, ResolvesCandidatesAcrossSmsAndGmailOrigins) {
  const OtpTarget target = AddOtpForm()[0];

  sms_otp_service_->SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "111111", base::TimeTicks::Now()));
  EXPECT_EQ(frame_observer_->otp_available_notifications(), 1u);
  EXPECT_EQ(DeliveredTokenValue(target), "111111");

  const OtpTarget other_origin_target =
      AddOtpForm(1, GURL("https://other.test"))[0];
  EXPECT_EQ(DeliveredTokenValue(other_origin_target), "111111");

  const base::Time t0 = base::Time::Now();
  SendGmailOtp("222222", t0 + base::Seconds(20));
  frame_observer_->WaitForNotifications(3u);
  EXPECT_EQ(DeliveredTokenValue(target), "222222");
  EXPECT_EQ(DeliveredTokenValue(other_origin_target), "111111");

  SendGmailOtp("000000", t0 + base::Seconds(5));
  SendGmailOtp("333333", t0 + base::Seconds(25), "noreply@other.test");
  frame_observer_->WaitForNotifications(4u);
  EXPECT_EQ(DeliveredTokenValue(target), "222222");
  EXPECT_EQ(DeliveredTokenValue(other_origin_target), "333333");
}

}  // namespace
}  // namespace autofill
