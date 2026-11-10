// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_coordinator.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
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
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_phish_guard_delegate.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_service.h"
#include "components/one_time_tokens/core/browser/fake_sms_otp_service.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "components/one_time_tokens/core/browser/one_time_token_type.h"
#include "components/one_time_tokens/core/common/one_time_token_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
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
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::Test;

constexpr char kTestUrl[] = "https://example.test";

auto HasValue(std::string_view code) {
  return ElementsAre(Property(&OneTimeToken::value, code));
}

class FakeOtpPhishGuardDelegate : public OtpPhishGuardDelegate {
 public:
  void StartOtpPhishGuardCheck(
      LocalFrameToken,
      base::OnceCallback<void(bool is_phishing)> callback) override {
    if (hold_callbacks_) {
      held_callbacks_.push_back(std::move(callback));
      if (on_check_) {
        std::move(on_check_).Run();
      }
      return;
    }
    std::move(callback).Run(/*is_phishing=*/false);
    ++completed_checks_;
    if (on_check_) {
      std::move(on_check_).Run();
    }
  }

  void HoldCallbacks() { hold_callbacks_ = true; }

  void WaitForChecks(size_t expected_additional = 1) {
    const size_t target = last_observed_checks_ + expected_additional;
    while (completed_checks_ < target) {
      base::test::TestFuture<void> future;
      on_check_ = future.GetCallback();
      ASSERT_TRUE(future.Wait());
    }
    last_observed_checks_ = completed_checks_;
  }

  void WaitForHeldCallbacks(size_t expected_count) {
    while (held_callbacks_.size() < expected_count) {
      base::test::TestFuture<void> future;
      on_check_ = future.GetCallback();
      ASSERT_TRUE(future.Wait());
    }
    ASSERT_EQ(held_callbacks_.size(), expected_count);
  }

  std::vector<base::OnceCallback<void(bool)>>& held_callbacks() {
    return held_callbacks_;
  }

 private:
  bool hold_callbacks_ = false;
  size_t completed_checks_ = 0;
  size_t last_observed_checks_ = 0;
  std::vector<base::OnceCallback<void(bool)>> held_callbacks_;
  base::OnceClosure on_check_;
};

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
  void TriggerSuggestions(FieldGlobalId field_id) override {
    triggered_fields_.push_back(field_id);
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
  const std::vector<FieldGlobalId>& triggered_fields() const {
    return triggered_fields_;
  }
  void clear_triggered_fields() { triggered_fields_.clear(); }

 private:
  const raw_ref<BrowserAutofillManager> manager_;
  const raw_ref<OtpCoordinator> coordinator_;
  size_t otp_available_notifications_ = 0;
  std::vector<FieldGlobalId> triggered_fields_;
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

    auto phish_guard = std::make_unique<FakeOtpPhishGuardDelegate>();
    phish_guard_delegate_ = phish_guard.get();
    autofill_client().set_otp_phish_guard_delegate(std::move(phish_guard));

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
    phish_guard_delegate_ = nullptr;
    gmail_service_ = nullptr;
    DestroyAutofillClient();
  }

  OtpCoordinatorTestApi test_api_coordinator() const {
    return OtpCoordinatorTestApi(*coordinator_);
  }

  std::vector<OtpTarget> AddOtpForm(size_t fields = 1,
                                    GURL url = GURL(kTestUrl),
                                    bool listen = true,
                                    bool user_typed = false) {
    FormDescription form_description = {
        .host_frame = autofill_driver().GetFrameToken(),
        .main_frame_origin = url::Origin::Create(url),
    };
    for (size_t i = 0; i < fields; ++i) {
      form_description.fields.push_back(
          {.server_type = ONE_TIME_CODE,
           .host_frame = autofill_driver().GetFrameToken(),
           .value = user_typed ? u"123" : u"",
           .origin = url::Origin::Create(url)});
    }
    FormData form = test::GetFormData(form_description);
    FormGlobalId form_id = form.global_id();
    auto form_structure = std::make_unique<FormStructure>(form);
    test_api(*form_structure).SetFieldTypes(GetServerTypes(form_description));
    test_api(*form_structure).AssignSections();
    if (user_typed) {
      for (size_t i = 0; i < fields; ++i) {
        form_structure->field(i)->AddFieldModifier(FieldModifier::kUser);
      }
    }
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

  FormData BuildFormWithFieldValue(const OtpTarget& target,
                                   std::string_view value) {
    FormData form =
        CHECK_DEREF(autofill_manager().FindCachedFormById(target.form))
            .ToFormData();
    for (FormFieldData& field : test_api(form).fields()) {
      if (field.global_id() == target.field) {
        field.set_value(base::UTF8ToUTF16(value));
      }
    }
    return form;
  }

  void SendGmailOtp(const std::string& code,
                    base::Time email_time,
                    std::optional<base::TimeTicks> arrival = std::nullopt,
                    const std::string& sender = "noreply@example.test") {
    if (!arrival.has_value()) {
      task_environment_.AdvanceClock(base::Milliseconds(1));
      arrival = base::TimeTicks::Now();
    }
    gmail_service_->ProcessCallbacks(OneTimeToken(
        OneTimeTokenType::kGmail, code, *arrival, sender, email_time));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  raw_ptr<one_time_tokens::FakeSmsOtpService> sms_otp_service_ = nullptr;
  raw_ptr<FakeOtpPhishGuardDelegate> phish_guard_delegate_ = nullptr;
  raw_ptr<one_time_tokens::FakeGmailOtpService> gmail_service_ = nullptr;
  std::unique_ptr<OtpCoordinator> coordinator_;
  std::unique_ptr<TestFrameObserver> frame_observer_;
};

TEST_F(OtpCoordinatorTest, IgnoresIneligibleTargets) {
  OtpTarget ineligible_target(
      url::Origin::Create(GURL(kTestUrl)), autofill_driver().GetFrameToken(),
      FormGlobalId(), FieldGlobalId(), autofill_driver().GetFrameToken());
  coordinator_->StartListeningForOtp(ineligible_target);
  EXPECT_THAT(coordinator_->GetAvailableTokens(ineligible_target), IsEmpty());
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
  const std::vector<OtpTarget> targets = AddOtpForm(2);
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 1u);
  coordinator_->OnTextFieldValueChanged();
  EXPECT_TRUE(test_api_coordinator()
                  .target_field_identifier()
                  .TypedInAnyFieldAfterOtpDetection());

  sms_otp_service_->SimulateOtpArrived(
      base::unexpected(OneTimeTokenRetrievalError::kSmsOtpBackendTimeout));
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 0u);

  coordinator_->StartListeningForOtp(targets[0]);
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 1u);
  EXPECT_TRUE(test_api_coordinator()
                  .target_field_identifier()
                  .TypedInAnyFieldAfterOtpDetection());
}

TEST_F(OtpCoordinatorTest,
       ReactiveDeliveryExpirationAndPrefGatingAcrossSmsAndGmailOrigins) {
  const OtpTarget target = AddOtpForm()[0];
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), IsEmpty());

  sms_otp_service_->SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "111111", base::TimeTicks::Now()));
  phish_guard_delegate_->WaitForChecks();
  EXPECT_EQ(frame_observer_->otp_available_notifications(), 1u);
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), HasValue("111111"));

  const OtpTarget same_site_second_target = AddOtpForm()[0];
  EXPECT_EQ(frame_observer_->otp_available_notifications(), 2u);
  EXPECT_THAT(coordinator_->GetAvailableTokens(same_site_second_target),
              HasValue("111111"));

  const OtpTarget other_origin_target =
      AddOtpForm(1, GURL("https://other.test"))[0];
  phish_guard_delegate_->WaitForChecks();
  EXPECT_EQ(frame_observer_->otp_available_notifications(), 3u);
  EXPECT_THAT(coordinator_->GetAvailableTokens(other_origin_target),
              HasValue("111111"));

  const base::Time t0 = base::Time::Now();
  SendGmailOtp("999000", t0 + base::Seconds(10),
               base::TimeTicks::Now() - base::Minutes(2));
  SendGmailOtp("222222", t0 + base::Seconds(20));
  phish_guard_delegate_->WaitForChecks();
  EXPECT_EQ(frame_observer_->otp_available_notifications(), 4u);
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), HasValue("222222"));
  EXPECT_THAT(coordinator_->GetAvailableTokens(other_origin_target),
              HasValue("111111"));

  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), false);
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), IsEmpty());
  prefs::SetAutofillGmailOtpFillingEnabled(autofill_client().GetPrefs(), true);
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), HasValue("222222"));

  SendGmailOtp("000000", t0 + base::Seconds(5));
  SendGmailOtp("333333", t0 + base::Seconds(25), std::nullopt,
               "noreply@other.test");
  phish_guard_delegate_->WaitForChecks();
  EXPECT_EQ(frame_observer_->otp_available_notifications(), 5u);
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), HasValue("222222"));
  EXPECT_THAT(coordinator_->GetAvailableTokens(other_origin_target),
              HasValue("333333"));

  // After 61 seconds, the 1-minute Gmail TTL expires (and a newly added field
  // on `other.test` does not inherit the expired Gmail candidate "333333"),
  // while the 3-minute SMS token ("111111") remains valid.
  const OtpTarget third_origin_target =
      AddOtpForm(1, GURL("https://third.test"))[0];
  phish_guard_delegate_->WaitForChecks();
  task_environment_.AdvanceClock(base::Seconds(61));
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), IsEmpty());
  EXPECT_THAT(coordinator_->GetAvailableTokens(other_origin_target), IsEmpty());
  const OtpTarget late_other_origin_target =
      AddOtpForm(1, GURL("https://other.test"))[0];
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(late_other_origin_target),
              HasValue("111111"));
  EXPECT_THAT(coordinator_->GetAvailableTokens(third_origin_target),
              HasValue("111111"));
}

TEST_F(OtpCoordinatorTest,
       DeduplicatesInFlightPhishGuardAndValidatesStrictlyNewerCandidate) {
  phish_guard_delegate_->HoldCallbacks();

  const std::vector<OtpTarget> targets = AddOtpForm(2);

  const base::Time t0 = base::Time::Now();
  const base::TimeTicks t1_arrival = base::TimeTicks::Now();
  SendGmailOtp("111111", t0 + base::Seconds(10), t1_arrival);
  phish_guard_delegate_->WaitForHeldCallbacks(1u);

  // Re-registering or receiving a duplicate/older candidate while PhishGuard is
  // in flight does not queue redundant checks, while a strictly newer candidate
  // queues a follow-up check.
  SendGmailOtp("111111", t0 + base::Seconds(10), t1_arrival);
  coordinator_->StartListeningForOtp(targets[0]);
  EXPECT_EQ(phish_guard_delegate_->held_callbacks().size(), 1u);

  SendGmailOtp("222222", t0 + base::Seconds(20));
  std::move(phish_guard_delegate_->held_callbacks()[0])
      .Run(/*is_phishing=*/false);
  frame_observer_->WaitForNotifications(1u);
  phish_guard_delegate_->WaitForHeldCallbacks(2u);

  std::move(phish_guard_delegate_->held_callbacks()[1])
      .Run(/*is_phishing=*/false);
  EXPECT_EQ(frame_observer_->otp_available_notifications(), 2u);
  EXPECT_THAT(coordinator_->GetAvailableTokens(targets[0]), HasValue("222222"));
  EXPECT_THAT(coordinator_->GetAvailableTokens(targets[1]), HasValue("222222"));

  frame_observer_.reset();
  EXPECT_THAT(coordinator_->GetAvailableTokens(targets[0]), IsEmpty());
}

TEST_F(OtpCoordinatorTest, PhishingVerdictBlocksSiteFrameAndClearsOriginCache) {
  phish_guard_delegate_->HoldCallbacks();

  const std::vector<OtpTarget> targets = AddOtpForm(2);

  const base::Time t0 = base::Time::Now();
  SendGmailOtp("333333", t0 + base::Seconds(30));
  phish_guard_delegate_->WaitForHeldCallbacks(1u);
  std::move(phish_guard_delegate_->held_callbacks()[0])
      .Run(/*is_phishing=*/true);
  EXPECT_THAT(coordinator_->GetAvailableTokens(targets[0]), IsEmpty());
  EXPECT_THAT(coordinator_->GetAvailableTokens(targets[1]), IsEmpty());

  // After a phishing verdict, subsequent candidates and newly added fields on
  // the same `(origin, frame)` remain blocked and the origin cache is cleared,
  // while other sites continue to validate and deliver candidates.
  const OtpTarget post_phishing_target = AddOtpForm(1)[0];
  SendGmailOtp("444444", t0 + base::Seconds(40));
  EXPECT_EQ(phish_guard_delegate_->held_callbacks().size(), 1u);
  const OtpTarget other_site_target =
      AddOtpForm(1, GURL("https://sub.example.test"))[0];
  phish_guard_delegate_->WaitForHeldCallbacks(2u);
  EXPECT_THAT(coordinator_->GetAvailableTokens(targets[0]), IsEmpty());
  EXPECT_THAT(coordinator_->GetAvailableTokens(post_phishing_target),
              IsEmpty());
  std::move(phish_guard_delegate_->held_callbacks()[1])
      .Run(/*is_phishing=*/false);
  EXPECT_THAT(coordinator_->GetAvailableTokens(other_site_target),
              HasValue("444444"));
}

TEST_F(OtpCoordinatorTest,
       PhishGuardTimeoutRejectsCandidateWithoutBlockingFrame) {
  phish_guard_delegate_->HoldCallbacks();

  const OtpTarget target = AddOtpForm()[0];
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(), target.form,
                               target.field);

  SendGmailOtp("111111", base::Time::Now());
  phish_guard_delegate_->WaitForHeldCallbacks(1u);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_THAT(frame_observer_->triggered_fields(), IsEmpty());
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), IsEmpty());

  SendGmailOtp("222222", base::Time::Now());
  phish_guard_delegate_->WaitForHeldCallbacks(2u);
  std::move(phish_guard_delegate_->held_callbacks()[1])
      .Run(/*is_phishing=*/false);
  EXPECT_THAT(frame_observer_->triggered_fields(), ElementsAre(target.field));
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), HasValue("222222"));
}

TEST_F(OtpCoordinatorTest, ServesCommandLineMockOtpWithoutStartingSources) {
  const OtpTarget target =
      AddOtpForm(/*fields=*/1, GURL(kTestUrl), /*listen=*/false)[0];

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      one_time_tokens::switches::kMockOtpValue, "999999");
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), HasValue("999999"));

  OtpCoordinator mock_coordinator(&autofill_client());
  TestFrameObserver mock_observer(&autofill_manager(), &mock_coordinator);
  mock_coordinator.StartListeningForOtp(target);
  EXPECT_EQ(sms_otp_service_->num_subscribers(), 0u);
  EXPECT_THAT(mock_coordinator.GetAvailableTokens(target), HasValue("999999"));
}

TEST_F(OtpCoordinatorTest,
       SuppressesUsedAndOlderCodesUntilExpirationAndOnFormSubmission) {
  const OtpTarget site1_target = AddOtpForm()[0];
  const std::vector<OtpTarget> site2_targets =
      AddOtpForm(/*fields=*/2, GURL("https://other.test"));
  EXPECT_EQ(gmail_service_->num_callbacks(), 2u);

  const base::Time t0 = base::Time::Now();
  // Arrive 2m50s ago, then fill now: used-token expiry is measured from
  // `max(on_device_arrival, recorded_at)` so re-polling 20s later (more than 3m
  // after `t_sms`, but only 20s after `recorded_at`) still suppresses "111111".
  const base::TimeTicks t_sms =
      base::TimeTicks::Now() - (base::Minutes(2) + base::Seconds(50));
  coordinator_->OnOtpFilled(site1_target, "000999");
  SendGmailOtp("000999", t0, t_sms - base::Seconds(3));
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target), IsEmpty());

  sms_otp_service_->SimulateOtpArrived(
      OneTimeToken(OneTimeTokenType::kSmsOtp, "111111", t_sms));
  phish_guard_delegate_->WaitForChecks(2u);
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target),
              HasValue("111111"));
  EXPECT_THAT(coordinator_->GetAvailableTokens(site2_targets[0]),
              HasValue("111111"));

  // Filling on `site1_target` consumes `site1_target`'s delivery and suppresses
  // older/matching codes on `site1_target.origin`, without consuming
  // `site2_targets`'s already-validated delivery on another origin.
  coordinator_->OnOtpFilled(site1_target, "111111");
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target), IsEmpty());
  EXPECT_THAT(coordinator_->GetAvailableTokens(site2_targets[0]),
              HasValue("111111"));

  // Advance 20s: `Now() - t_sms` is now 3m10s (> 3m), while `Now() -
  // recorded_at` is 20s (< 3m). Re-polling "111111" must still be suppressed.
  task_environment_.AdvanceClock(base::Seconds(20));
  SendGmailOtp("000000", t0 + base::Seconds(5), t_sms - base::Seconds(5));
  SendGmailOtp("111111", t0, base::TimeTicks::Now());
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target), IsEmpty());

  SendGmailOtp("222222", t0 + base::Seconds(30), base::TimeTicks::Now());
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target),
              HasValue("222222"));

  // Submitting with a different typed value (e.g. a typo) drops the form's
  // delivery and records the typed value, without consuming the offered code
  // "222222". Re-listening on `site1_target` re-delivers "222222".
  coordinator_->OnOtpFormSubmitted(
      BuildFormWithFieldValue(site1_target, "999888"));
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target), IsEmpty());
  EXPECT_EQ(gmail_service_->num_callbacks(), 1u);

  coordinator_->StartListeningForOtp(site1_target);
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target),
              HasValue("222222"));

  // Submitting with the offered value "222222" consumes "222222", so
  // re-listening on `site1_target` keeps "222222" suppressed until its
  // 1-minute used-code TTL expires.
  coordinator_->OnOtpFormSubmitted(
      BuildFormWithFieldValue(site1_target, "222222"));
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target), IsEmpty());
  coordinator_->StartListeningForOtp(site1_target);
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target), IsEmpty());

  task_environment_.AdvanceClock(base::Minutes(2));
  SendGmailOtp("222222", base::Time::Now());
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(site1_target),
              HasValue("222222"));

  // Submitting a split single-character multi-box form on `site2_targets`
  // concatenates its field values ("4" + "2" -> "42") and suppresses "42" on
  // `site2_targets`'s origin.
  FormData split_form =
      CHECK_DEREF(autofill_manager().FindCachedFormById(site2_targets[0].form))
          .ToFormData();
  test_api(split_form).field(0).set_value(u"4");
  test_api(split_form).field(1).set_value(u"2");
  coordinator_->OnOtpFormSubmitted(split_form);
  coordinator_->StartListeningForOtp(site2_targets[0]);
  sms_otp_service_->SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "42", base::TimeTicks::Now()));
  EXPECT_THAT(coordinator_->GetAvailableTokens(site2_targets[0]), IsEmpty());
}

TEST_F(OtpCoordinatorTest,
       HandlesStopListeningAndFormSubmissionDuringInFlightPhishGuard) {
  const OtpTarget site1_target = AddOtpForm()[0];
  const std::vector<OtpTarget> site2_targets =
      AddOtpForm(/*fields=*/2, GURL("https://sub.example.test"));
  EXPECT_EQ(gmail_service_->num_callbacks(), 2u);

  // Hold PhishGuard while `site1_target` keeps `listening_generation_` alive
  // and `site2_targets[0]` is removed via `StopListeningForForm(..., except)`,
  // then verify the remaining active field `site2_targets[1]` still receives
  // the validated candidate.
  phish_guard_delegate_->HoldCallbacks();

  SendGmailOtp("333333", base::Time::Now() + base::Seconds(10));
  phish_guard_delegate_->WaitForHeldCallbacks(1u);

  coordinator_->StopListeningForForm(site2_targets[0].form,
                                     /*except=*/{site2_targets[1].field});
  std::move(phish_guard_delegate_->held_callbacks()[0])
      .Run(/*is_phishing=*/false);
  phish_guard_delegate_->WaitForHeldCallbacks(2u);
  std::move(phish_guard_delegate_->held_callbacks()[1])
      .Run(/*is_phishing=*/false);
  EXPECT_THAT(coordinator_->GetAvailableTokens(site2_targets[0]), IsEmpty());
  EXPECT_THAT(coordinator_->GetAvailableTokens(site2_targets[1]),
              HasValue("333333"));

  coordinator_->OnOtpFormSubmitted(
      BuildFormWithFieldValue(site2_targets[1], "333333"));
  EXPECT_THAT(coordinator_->GetAvailableTokens(site2_targets[1]), IsEmpty());

  const OtpTarget site2_after_submit_target =
      AddOtpForm(/*fields=*/1, GURL("https://sub.example.test"))[0];
  coordinator_->StopListeningForForm(site1_target.form);
  EXPECT_EQ(gmail_service_->num_callbacks(), 1u);
  coordinator_->StopListeningForForm(site2_after_submit_target.form);
  EXPECT_EQ(gmail_service_->num_callbacks(), 0u);
  coordinator_->StartListeningForOtp(site2_after_submit_target);
  EXPECT_EQ(gmail_service_->num_callbacks(), 1u);
}

TEST_F(OtpCoordinatorTest,
       ProactiveTriggerTargetsFocusedFieldAndOncePerFormAndCode) {
  const std::vector<OtpTarget> targets = AddOtpForm(2);
  const OtpTarget& field0 = targets[0];
  const OtpTarget& field1 = targets[1];

  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(), field1.form,
                               field1.field);
  sms_otp_service_->SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "654321", base::TimeTicks::Now()));
  phish_guard_delegate_->WaitForChecks();

  EXPECT_THAT(coordinator_->GetAvailableTokens(field0), HasValue("654321"));
  EXPECT_THAT(coordinator_->GetAvailableTokens(field1), HasValue("654321"));
  EXPECT_THAT(frame_observer_->triggered_fields(), ElementsAre(field1.field));

  // Registering a second 2-field form on the same site when "654321" is already
  // available notifies `OnOtpAvailable` once and triggers proactively on the
  // focused field `second_form_targets[1]` (not once per sibling field).
  EXPECT_EQ(frame_observer_->otp_available_notifications(), 1u);
  frame_observer_->clear_triggered_fields();
  const std::vector<OtpTarget> second_form_targets =
      AddOtpForm(/*fields=*/2, GURL(kTestUrl), /*listen=*/false);
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(),
                               second_form_targets[1].form,
                               second_form_targets[1].field);
  coordinator_->StartListeningForOtp(second_form_targets[0]);
  coordinator_->StartListeningForOtp(second_form_targets[1]);
  EXPECT_EQ(frame_observer_->otp_available_notifications(), 2u);
  EXPECT_THAT(frame_observer_->triggered_fields(),
              ElementsAre(second_form_targets[1].field));
}

TEST_F(OtpCoordinatorTest,
       OnOtpSuggestionsShownSuppressesProactiveTriggerForSameCode) {
  const std::vector<OtpTarget> targets = AddOtpForm(2);
  const OtpTarget& field0 = targets[0];
  const base::Time t0 = base::Time::Now();

  // Focus a non-OTP field so the initial arrival of "654321" is validated and
  // stored without triggering a proactive popup.
  FieldGlobalId non_otp_field(autofill_driver().GetFrameToken(),
                              FieldRendererId(999));
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(), field0.form,
                               non_otp_field);
  SendGmailOtp("654321", t0);
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(field0), HasValue("654321"));
  EXPECT_THAT(frame_observer_->triggered_fields(), IsEmpty());

  // Mark `field0` as shown (`OnOtpSuggestionsShown` records across all fields
  // of `field0.form`), focus `targets[1]`, and resend "654321": no proactive
  // trigger should fire for the already-shown code on either field.
  coordinator_->OnOtpSuggestionsShown(field0.field, "000000");
  coordinator_->OnOtpSuggestionsShown(field0.field, "654321");
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(),
                               targets[1].form, targets[1].field);
  SendGmailOtp("654321", t0 + base::Seconds(10));
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(frame_observer_->triggered_fields(), IsEmpty());
}

TEST_F(OtpCoordinatorTest, SuppressesProactiveTriggerOnNonOtpFocus) {
  const OtpTarget target = AddOtpForm()[0];
  const base::Time t0 = base::Time::Now();

  FieldGlobalId non_otp_field(autofill_driver().GetFrameToken(),
                              FieldRendererId(999));
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(), target.form,
                               non_otp_field);
  SendGmailOtp("888999", t0 + base::Seconds(10));
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), HasValue("888999"));
  EXPECT_THAT(frame_observer_->triggered_fields(), IsEmpty());

  const OtpTarget new_target =
      AddOtpForm(/*fields=*/1, GURL(kTestUrl), /*listen=*/false)[0];
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(),
                               new_target.form, new_target.field);
  coordinator_->StartListeningForOtp(new_target);
  EXPECT_THAT(coordinator_->GetAvailableTokens(new_target), HasValue("888999"));
  EXPECT_THAT(frame_observer_->triggered_fields(),
              ElementsAre(new_target.field));
}

TEST_F(OtpCoordinatorTest, SuppressesProactiveTriggerAfterUserTyping) {
  const OtpTarget target = AddOtpForm()[0];
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(), target.form,
                               target.field);
  coordinator_->OnTextFieldValueChanged();

  SendGmailOtp("901234", base::Time::Now());
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), HasValue("901234"));
  EXPECT_THAT(frame_observer_->triggered_fields(), IsEmpty());
}

TEST_F(OtpCoordinatorTest,
       SuppressesProactiveTriggerWhenOtpFieldContainsUserTypedInput) {
  const OtpTarget typed_target =
      AddOtpForm(/*fields=*/1, GURL(kTestUrl), /*listen=*/true,
                 /*user_typed=*/true)[0];
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(),
                               typed_target.form, typed_target.field);

  SendGmailOtp("956789", base::Time::Now());
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(typed_target),
              HasValue("956789"));
  EXPECT_THAT(frame_observer_->triggered_fields(), IsEmpty());
}

TEST_F(OtpCoordinatorTest,
       DefersProactiveTriggerUntilTabVisibleAndWindowFocused) {
  const std::vector<OtpTarget> targets = AddOtpForm(2);
  const base::Time t0 = base::Time::Now();

  // While hidden and unfocused (even if a non-OTP field had focus while
  // backgrounded), arriving OTPs are deferred without evaluating live focus
  // policy prematurely.
  FieldGlobalId non_otp_field(autofill_driver().GetFrameToken(),
                              FieldRendererId(999));
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(),
                               targets[0].form, non_otp_field);
  coordinator_->OnTabVisibilityChanged(false);
  coordinator_->OnWindowFocusChanged(false);

  SendGmailOtp("314159", t0 + base::Seconds(10));
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(targets[0]), HasValue("314159"));
  EXPECT_THAT(frame_observer_->triggered_fields(), IsEmpty());

  // Tab becomes visible while window is still unfocused and focus moves to the
  // second OTP field `targets[1]`: still deferred.
  coordinator_->OnFocusChanged(autofill_driver().GetFrameToken(),
                               targets[1].form, targets[1].field);
  coordinator_->OnTabVisibilityChanged(true);
  EXPECT_THAT(frame_observer_->triggered_fields(), IsEmpty());

  // Window regains focus: deferred trigger now fires once on the newly focused
  // field `targets[1]`.
  coordinator_->OnWindowFocusChanged(true);
  EXPECT_THAT(frame_observer_->triggered_fields(),
              ElementsAre(targets[1].field));
}

TEST_F(OtpCoordinatorTest, OnFrameBecameInactiveDropsDeliveries) {
  const OtpTarget target = AddOtpForm()[0];
  sms_otp_service_->SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "111111", base::TimeTicks::Now()));
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), HasValue("111111"));

  autofill_driver().SetIsActive(false);
  coordinator_->OnFrameBecameInactive(autofill_driver().GetFrameToken());
  EXPECT_THAT(test_api_coordinator().deliveries(), IsEmpty());
  EXPECT_THAT(coordinator_->GetAvailableTokens(target), IsEmpty());
}

TEST_F(OtpCoordinatorTest, RecordsGmailSuggestionReadyLatency) {
  constexpr char kHistogram[] =
      "OneTimeTokens.Gmail.FieldDetectionToOtpSuggestionReadyLatency";
  base::HistogramTester histogram_tester;

  gmail_service_->SetCachedTokens(
      {OneTimeToken(OneTimeTokenType::kGmail, "111222", base::TimeTicks::Now(),
                    "noreply@example.test", base::Time::Now())});
  task_environment_.FastForwardBy(base::Seconds(2));

  const std::vector<OtpTarget> targets = AddOtpForm(2);
  phish_guard_delegate_->WaitForChecks();
  EXPECT_THAT(coordinator_->GetAvailableTokens(targets[0]), HasValue("111222"));
  histogram_tester.ExpectTotalCount(kHistogram, 0);

  sms_otp_service_->SimulateOtpArrived(OneTimeToken(
      OneTimeTokenType::kSmsOtp, "333444", base::TimeTicks::Now()));
  phish_guard_delegate_->WaitForChecks();
  histogram_tester.ExpectTotalCount(kHistogram, 0);

  task_environment_.FastForwardBy(base::Milliseconds(1500));
  SendGmailOtp("123456", base::Time::Now(), base::TimeTicks::Now());
  phish_guard_delegate_->WaitForChecks();
  histogram_tester.ExpectUniqueTimeSample(kHistogram, base::Milliseconds(1500),
                                          1);

  task_environment_.FastForwardBy(base::Milliseconds(500));
  SendGmailOtp("654321", base::Time::Now(), base::TimeTicks::Now());
  phish_guard_delegate_->WaitForChecks();
  histogram_tester.ExpectTotalCount(kHistogram, 1);

  coordinator_->StopListeningForForm(targets[0].form);
  test_api(autofill_manager()).ClearFormStructures();
  AddOtpForm();
  phish_guard_delegate_->WaitForChecks(2);
  task_environment_.FastForwardBy(base::Milliseconds(800));
  SendGmailOtp("777888", base::Time::Now(), base::TimeTicks::Now());
  phish_guard_delegate_->WaitForChecks();
  histogram_tester.ExpectTotalCount(kHistogram, 2);
  histogram_tester.ExpectBucketCount(kHistogram, 800, 1);
}

}  // namespace
}  // namespace autofill
