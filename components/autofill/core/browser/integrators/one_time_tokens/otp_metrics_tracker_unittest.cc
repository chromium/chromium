// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/one_time_tokens/core/browser/gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/mock_one_time_token_service.h"
#include "components/one_time_tokens/core/browser/one_time_token_service_constants.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::NiceMock;

class OtpMetricsTrackerTest : public testing::Test {
 public:
  OtpMetricsTrackerTest() = default;

  void SetUp() override {
    ON_CALL(mock_ott_service_,
            SubscribeToTickles(one_time_tokens::OneTimeTokenSource::kGmail,
                               base::Time::Max(), _))
        .WillByDefault(
            [this](one_time_tokens::OneTimeTokenSource, base::Time exp,
                   one_time_tokens::OneTimeTokenService::TickleCallback cb) {
              return subscription_manager_.Subscribe(
                  exp, std::move(cb),
                  /*expiration_callback=*/base::DoNothing());
            });
  }

 protected:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_{features::kAutofillGmailOtp};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service_;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      subscription_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(OtpMetricsTrackerTest, NullServiceDoesNotCrash) {
  OtpMetricsTracker tracker(/*one_time_token_service=*/nullptr);
  EXPECT_FALSE(tracker.HasActiveSubscriptionForTesting());
}

TEST_F(OtpMetricsTrackerTest, SubscribesUponConstruction) {
  EXPECT_CALL(mock_ott_service_,
              SubscribeToTickles(one_time_tokens::OneTimeTokenSource::kGmail,
                                 base::Time::Max(), _))
      .WillOnce(
          [this](one_time_tokens::OneTimeTokenSource, base::Time exp,
                 one_time_tokens::OneTimeTokenService::TickleCallback cb) {
            return subscription_manager_.Subscribe(
                exp, std::move(cb), /*expiration_callback=*/base::DoNothing());
          });

  OtpMetricsTracker tracker(&mock_ott_service_);
  EXPECT_TRUE(tracker.HasActiveSubscriptionForTesting());
}

TEST_F(
    OtpMetricsTrackerTest,
    FieldDetectionToTickleLatency_LoggedWhenTickleArrivesAfterFieldDetection) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  task_environment_.FastForwardBy(base::Milliseconds(500));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(500), 1);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_NotLoggedIfNoFieldDetected) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_OnlyFirstTickleLogged) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  task_environment_.FastForwardBy(base::Milliseconds(200));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  task_environment_.FastForwardBy(base::Milliseconds(300));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(200), 1);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_LastFieldDetectionTimestampUsed) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  task_environment_.FastForwardBy(base::Milliseconds(100));
  // Subsequent field detections update the timestamp to the last seen field.
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  task_environment_.FastForwardBy(base::Milliseconds(200));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(200), 1);
}

TEST_F(
    OtpMetricsTrackerTest,
    FieldDetectionToTickleLatency_NotLoggedIfMoreThanFieldDetectionTimeoutPass) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  task_environment_.FastForwardBy(OtpMetricsTracker::kFieldDetectionTimeout +
                                  base::Milliseconds(1));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_NewSessionAfterTickle) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  // First session.
  tracker.OnOtpFieldDetected(test::MakeFormGlobalId(), {}, nullptr);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  // Second session.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  tracker.OnOtpFieldDetected(test::MakeFormGlobalId(), {}, nullptr);
  task_environment_.FastForwardBy(base::Milliseconds(250));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 2);
  histogram_tester_.ExpectTimeBucketCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(100), 1);
  histogram_tester_.ExpectTimeBucketCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(250), 1);
}

TEST_F(OtpMetricsTrackerTest,
       FieldDetectionToTickleLatency_NotLoggedIfFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(features::kAutofillGmailOtp);

  OtpMetricsTracker tracker(&mock_ott_service_);
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  task_environment_.FastForwardBy(base::Milliseconds(500));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest,
       TickleToFieldDetectionLatency_LoggedWhenFieldDetectedAfterTickle) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      base::Milliseconds(300), 1);
  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram, 0);
}

TEST_F(
    OtpMetricsTrackerTest,
    TickleToFieldDetectionLatency_NotLoggedIfMoreThanFieldDetectionTimeoutPass) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(OtpMetricsTracker::kFieldDetectionTimeout +
                                  base::Milliseconds(1));
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest,
       TickleToFieldDetectionLatency_LastTickleTimestampUsed) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  // First tickle.
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(100));

  // Second tickle: updates tickle timestamp.
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(250));

  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      base::Milliseconds(250), 1);
}

TEST_F(OtpMetricsTrackerTest,
       TickleToFieldDetectionLatency_OnlyFirstFieldDetectionLogged) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(150));

  // First field detection: records latency and resets tickle timestamp.
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  task_environment_.FastForwardBy(base::Milliseconds(200));
  // Subsequent field detection: should not record again.
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      base::Milliseconds(150), 1);
}

TEST_F(OtpMetricsTrackerTest,
       TickleToFieldDetectionLatency_NotLoggedIfFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(features::kAutofillGmailOtp);

  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  histogram_tester_.ExpectTotalCount(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram, 0);
}

TEST_F(OtpMetricsTrackerTest, FieldDetectionAndTickle_BidirectionalSessions) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  // Session 1: Tickle arrives first, then field detected.
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(150));
  tracker.OnOtpFieldDetected(test::MakeFormGlobalId(), {}, nullptr);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kTickleToFieldDetectionLatencyHistogram,
      base::Milliseconds(150), 1);

  // Session 2: Field detected first, then tickle arrives.
  task_environment_.FastForwardBy(base::Milliseconds(500));
  tracker.OnOtpFieldDetected(test::MakeFormGlobalId(), {}, nullptr);
  task_environment_.FastForwardBy(base::Milliseconds(250));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueTimeSample(
      OtpMetricsTracker::kFieldDetectionToTickleLatencyHistogram,
      base::Milliseconds(250), 1);
}

TEST_F(OtpMetricsTrackerTest, TickleArrival_AfterFieldDetection) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleArrivalHistogram,
      one_time_tokens::TickleArrival::kAfterFieldDetection, 1);
}

TEST_F(OtpMetricsTrackerTest, TickleArrival_BeforeFieldDetection) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleArrivalHistogram,
      one_time_tokens::TickleArrival::kBeforeFieldDetection, 1);
}

TEST_F(OtpMetricsTrackerTest, TickleArrival_WithoutFieldDetection) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  // Fast forward past expiration without detecting any field.
  task_environment_.FastForwardBy(
      one_time_tokens::kNotificationExpirationDuration);

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleArrivalHistogram,
      one_time_tokens::TickleArrival::kWithoutFieldDetection, 1);
}

TEST_F(OtpMetricsTrackerTest,
       TickleArrival_WithoutFieldDetection_TimerCancelledByFieldDetection) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);

  // Fast forward beyond the original 3-minute window.
  task_environment_.FastForwardBy(base::Minutes(3));

  // Should only have recorded kBeforeFieldDetection, not
  // kWithoutFieldDetection.
  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleArrivalHistogram,
      one_time_tokens::TickleArrival::kBeforeFieldDetection, 1);
}

TEST_F(OtpMetricsTrackerTest,
       TickleArrival_WithoutFieldDetection_SubsequentTickleExtendsTimer) {
  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Minutes(2));

  // Second tickle restarts the 3-minute timer.
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Minutes(2));

  // 4 minutes total elapsed since first tickle, but only 2 minutes since second
  // tickle.
  histogram_tester_.ExpectTotalCount(one_time_tokens::kTickleArrivalHistogram,
                                     0);

  // 1 more minute (3 minutes since second tickle) -> timer fires.
  task_environment_.FastForwardBy(base::Minutes(1));
  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleArrivalHistogram,
      one_time_tokens::TickleArrival::kWithoutFieldDetection, 1);
}

TEST_F(OtpMetricsTrackerTest, TickleArrival_FeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(features::kAutofillGmailOtp);

  OtpMetricsTracker tracker(&mock_ott_service_);

  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  tracker.OnOtpFieldDetected(FormGlobalId{}, {}, nullptr);
  task_environment_.FastForwardBy(
      one_time_tokens::kNotificationExpirationDuration);

  histogram_tester_.ExpectTotalCount(one_time_tokens::kTickleArrivalHistogram,
                                     0);
}

class OtpMetricsTrackerFormOutcomeTest
    : public OtpMetricsTrackerTest,
      public WithTestAutofillClientDriverManager<> {
 public:
  void SetUp() override {
    OtpMetricsTrackerTest::SetUp();
    InitAutofillClient();
    CreateAutofillDriver();
  }

  const FormStructure* AddFormWithOtpField() {
    FormData form = test::GetFormData({.fields = {{.role = ONE_TIME_CODE}}});
    FormGlobalId form_id = form.global_id();
    auto form_structure = std::make_unique<FormStructure>(form);
    form_structure->field(0)->SetTypeTo(AutofillType(ONE_TIME_CODE),
                                        std::nullopt);
    test_api(autofill_manager())
        .AddSeenFormStructure(std::move(form_structure));
    test_api(autofill_manager()).OnFormsParsed({form});

    autofill_manager().NotifyObservers(
        &TestBrowserAutofillManager::Observer::OnFieldTypesDetermined, form_id,
        TestBrowserAutofillManager::Observer::FieldTypeSource::kAutofillAiModel,
        /*small_forms_were_parsed=*/false);
    return autofill_manager().FindCachedFormById(form_id);
  }
};

TEST_F(OtpMetricsTrackerFormOutcomeTest, TickleBeforeUserInteraction) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  const FormStructure* form = AddFormWithOtpField();

  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  task_environment_.FastForwardBy(base::Milliseconds(300));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleFormOutcomeHistogram,
      one_time_tokens::TickleFormOutcome::kTickleBeforeUserInteraction, 1);
}

TEST_F(OtpMetricsTrackerFormOutcomeTest,
       TickleAfterUserInteraction_FieldHasValue) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  const FormStructure* form = AddFormWithOtpField();

  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  // User typed in the field before tickle arrived.
  const_cast<AutofillField*>(form->field(0))->set_value(u"123456");

  task_environment_.FastForwardBy(base::Milliseconds(300));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleFormOutcomeHistogram,
      one_time_tokens::TickleFormOutcome::kTickleAfterUserInteraction, 1);
}

TEST_F(OtpMetricsTrackerFormOutcomeTest,
       TickleAfterUserInteraction_FieldModifiedByUser) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  const FormStructure* form = AddFormWithOtpField();

  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  // Field was modified by user.
  const_cast<AutofillField*>(form->field(0))
      ->AddFieldModifier(FieldModifier::kUser);

  task_environment_.FastForwardBy(base::Milliseconds(300));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleFormOutcomeHistogram,
      one_time_tokens::TickleFormOutcome::kTickleAfterUserInteraction, 1);
}

TEST_F(OtpMetricsTrackerFormOutcomeTest,
       TickleAfterUserInteraction_FrameDestroyed) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  const FormStructure* form = AddFormWithOtpField();

  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  // User submitted or navigated away -> frame / BrowserAutofillManager
  // destroyed.
  DestroyAutofillClient();

  task_environment_.FastForwardBy(base::Milliseconds(300));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleFormOutcomeHistogram,
      one_time_tokens::TickleFormOutcome::kTickleAfterUserInteraction, 1);
}

TEST_F(OtpMetricsTrackerFormOutcomeTest, NoTickleReceived_Timeout) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  const FormStructure* form = AddFormWithOtpField();

  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  // 3 minutes elapse with no tickle.
  task_environment_.FastForwardBy(
      one_time_tokens::kNotificationExpirationDuration);

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleFormOutcomeHistogram,
      one_time_tokens::TickleFormOutcome::kNoTickleReceived, 1);
}

TEST_F(OtpMetricsTrackerFormOutcomeTest,
       PreArrival_TickleBeforeFieldDetection) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  const FormStructure* form = AddFormWithOtpField();

  // Tickle arrives before field detection.
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(base::Milliseconds(300));

  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleFormOutcomeHistogram,
      one_time_tokens::TickleFormOutcome::kTickleBeforeUserInteraction, 1);
}

TEST_F(OtpMetricsTrackerFormOutcomeTest, FeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(features::kAutofillGmailOtp);

  OtpMetricsTracker tracker(&mock_ott_service_);
  const FormStructure* form = AddFormWithOtpField();

  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());
  task_environment_.FastForwardBy(base::Milliseconds(300));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  task_environment_.FastForwardBy(
      one_time_tokens::kNotificationExpirationDuration);

  histogram_tester_.ExpectTotalCount(
      one_time_tokens::kTickleFormOutcomeHistogram, 0);
}

TEST_F(OtpMetricsTrackerFormOutcomeTest,
       RepeatedFieldDetection_FormOutcomeAlreadyRecorded_DoesNotDuplicate) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  const FormStructure* form = AddFormWithOtpField();

  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  task_environment_.FastForwardBy(base::Milliseconds(300));
  subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);

  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleFormOutcomeHistogram,
      one_time_tokens::TickleFormOutcome::kTickleBeforeUserInteraction, 1);

  // Form is re-parsed/re-detected (e.g. server predictions or DOM mutation).
  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  // 3 minutes elapse after the re-detection.
  task_environment_.FastForwardBy(
      one_time_tokens::kNotificationExpirationDuration);

  // Should NOT record a duplicate sample (e.g. kNoTickleReceived).
  histogram_tester_.ExpectTotalCount(
      one_time_tokens::kTickleFormOutcomeHistogram, 1);
}

TEST_F(OtpMetricsTrackerFormOutcomeTest,
       RepeatedFieldDetection_PendingForm_DoesNotResetTimer) {
  OtpMetricsTracker tracker(&mock_ott_service_);
  const FormStructure* form = AddFormWithOtpField();

  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  // 1 minute passes, then form is re-detected (e.g. server predictions return).
  task_environment_.FastForwardBy(base::Minutes(1));
  tracker.OnOtpFieldDetected(form->global_id(), {form->field(0)->global_id()},
                             autofill_manager().GetWeakPtr());

  // Fast forward 2 more minutes (total 3 minutes from initial detection).
  task_environment_.FastForwardBy(base::Minutes(2));

  // The timer should have fired at 3 minutes from original detection, not 4.
  histogram_tester_.ExpectUniqueSample(
      one_time_tokens::kTickleFormOutcomeHistogram,
      one_time_tokens::TickleFormOutcome::kNoTickleReceived, 1);
}

}  // namespace
}  // namespace autofill
