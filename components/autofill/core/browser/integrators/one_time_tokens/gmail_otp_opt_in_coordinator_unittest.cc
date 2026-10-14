// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_coordinator.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_result.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/fake_gmail_otp_backend.h"
#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"
#include "components/signin/public/base/consent_level.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Test;

class TestOtpFieldDetector : public OtpFieldDetector {
 public:
  TestOtpFieldDetector() : OtpFieldDetector(nullptr) {}
  ~TestOtpFieldDetector() override = default;

  using OtpFieldDetector::AddFormAndNotifyIfNecessary;
  using OtpFieldDetector::RemoveFormAndNotifyIfNecessary;
};

class MockOptInAutofillClient : public TestAutofillClient {
 public:
  MockOptInAutofillClient() = default;
  ~MockOptInAutofillClient() override = default;

  MOCK_METHOD(void,
              ShowGmailOtpOptInBubble,
              (const std::u16string& account_email,
               base::OnceCallback<void(GmailOtpOptInResult)> callback),
              (override));
};

class TestSessionUserData : public base::SupportsUserData {
 public:
  TestSessionUserData() = default;
  ~TestSessionUserData() override = default;
};

class GmailOtpOptInCoordinatorTest : public Test {
 public:
  GmailOtpOptInCoordinatorTest() {
    auto detector = std::make_unique<TestOtpFieldDetector>();
    detector_ = detector.get();
    client_.set_otp_field_detector(std::move(detector));
  }

  void SignIn(const std::string& email = "user@gmail.com") {
    client_.identity_test_environment().MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
  }

  void SimulateOtpFieldDetected() {
    detector_->AddFormAndNotifyIfNecessary(test::MakeFormGlobalId());
  }

  void SimulateTickleReceived() {
    one_time_tokens::OneTimeTokenBackendNotification notification;
    notification.notification_received_timeticks = base::TimeTicks::Now();
    gmail_otp_backend_.OnIncomingOneTimeTokenBackendNotification(notification);
  }

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockOptInAutofillClient> client_;
  raw_ptr<TestOtpFieldDetector> detector_ = nullptr;
  one_time_tokens::FakeGmailOtpBackend gmail_otp_backend_;
  TestSessionUserData session_user_data_;
};

TEST_F(GmailOtpOptInCoordinatorTest, NullBackendDoesNotCrash) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_,
                                       /*gmail_otp_backend=*/nullptr,
                                       session_user_data_);
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateOtpFieldDetected();
}

// Verifies that the coordinator does not subscribe to tickles until an OTP
// field is detected on the page.
TEST_F(GmailOtpOptInCoordinatorTest,
       DoesNotSubscribeToTicklesUntilOtpFieldDetected) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);
  EXPECT_EQ(gmail_otp_backend_.num_tickle_callbacks(), 0u);

  SimulateOtpFieldDetected();
  EXPECT_EQ(gmail_otp_backend_.num_tickle_callbacks(), 1u);
}

// Verifies that the tickle subscription expires `kTickleSubscriptionDuration`
// after the OTP field was detected.
TEST_F(GmailOtpOptInCoordinatorTest,
       TickleSubscriptionExpiresAfterSubscriptionDuration) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);
  SimulateOtpFieldDetected();

  task_environment_.FastForwardBy(
      GmailOtpOptInCoordinator::kTickleSubscriptionDuration - base::Seconds(1));
  EXPECT_EQ(gmail_otp_backend_.num_tickle_callbacks(), 1u);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(gmail_otp_backend_.num_tickle_callbacks(), 0u);
}

// Scenario A: The OTP form is detected first, and a Gmail tickle arrives later
// within the subscription window.
TEST_F(GmailOtpOptInCoordinatorTest,
       TriggersWhenOtpFieldDetectedBeforeTickleReceived) {
  SignIn("alice@gmail.com");
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  SimulateOtpFieldDetected();
  task_environment_.FastForwardBy(base::Minutes(1));

  EXPECT_CALL(client_,
              ShowGmailOtpOptInBubble(std::u16string(u"alice@gmail.com"), _));
  SimulateTickleReceived();
}

// Scenario B: A Gmail tickle arrived and was cached in `GmailOtpBackendImpl`
// before the OTP form was detected, so `SubscribeToTickles` synchronously
// invokes the callback when `OnOtpFieldsDetected` subscribes.
TEST_F(GmailOtpOptInCoordinatorTest,
       TriggersWhenCachedTickleDeliveredOnOtpFieldDetection) {
  SignIn("bob@gmail.com");
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);
  SimulateTickleReceived();

  EXPECT_CALL(client_,
              ShowGmailOtpOptInBubble(std::u16string(u"bob@gmail.com"), _));
  SimulateOtpFieldDetected();
}

// Verifies that if an OTP field is already present when the coordinator is
// constructed, it immediately subscribes to tickles and triggers the bubble if
// a cached tickle is delivered synchronously.
TEST_F(GmailOtpOptInCoordinatorTest,
       SubscribesAndTriggersOnConstructionWhenOtpFieldAlreadyPresent) {
  SignIn("sync@gmail.com");
  SimulateOtpFieldDetected();
  SimulateTickleReceived();

  EXPECT_CALL(client_,
              ShowGmailOtpOptInBubble(std::u16string(u"sync@gmail.com"), _));
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);
}

// Verifies that once `kTickleSubscriptionDuration` elapses after OTP field
// detection, the tickle subscription expires and subsequent tickles do not
// trigger the bubble.
TEST_F(GmailOtpOptInCoordinatorTest,
       DoesNotTriggerAfterTickleSubscriptionExpires) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  SimulateOtpFieldDetected();
  task_environment_.FastForwardBy(
      GmailOtpOptInCoordinator::kTickleSubscriptionDuration + base::Seconds(1));

  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateTickleReceived();
}

// Verifies that detecting a new OTP form while the subscription is still alive
// extends its expiration time rather than creating a duplicate subscription.
TEST_F(GmailOtpOptInCoordinatorTest,
       ExtendsSubscriptionWhenNewOtpFormDetected) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  FormGlobalId form1 = test::MakeFormGlobalId();
  detector_->AddFormAndNotifyIfNecessary(form1);
  EXPECT_EQ(gmail_otp_backend_.num_tickle_callbacks(), 1u);

  // Advance 2 minutes, then navigate to a new OTP form (0 -> 1 transition).
  task_environment_.FastForwardBy(base::Minutes(2));
  detector_->RemoveFormAndNotifyIfNecessary(form1);
  detector_->AddFormAndNotifyIfNecessary(test::MakeFormGlobalId());
  EXPECT_EQ(gmail_otp_backend_.num_tickle_callbacks(), 1u);

  // Advance another 2 minutes (4 minutes total since form1, but 2 minutes since
  // form2). The extended subscription should still be alive.
  task_environment_.FastForwardBy(base::Minutes(2));
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(1);
  SimulateTickleReceived();
}

// Gating check: Do not show if `kAutofillGmailOtpFillingEnabled` is already
// true.
TEST_F(GmailOtpOptInCoordinatorTest,
       DoesNotTriggerWhenGmailOtpFillingAlreadyEnabled) {
  SignIn();
  prefs::SetAutofillGmailOtpFillingEnabled(client_.GetPrefs(), true);
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateOtpFieldDetected();
  SimulateTickleReceived();
}

// Gating check: Do not show if there is no signed-in primary account.
TEST_F(GmailOtpOptInCoordinatorTest, DoesNotTriggerWhenNotSignedIn) {
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateOtpFieldDetected();
  SimulateTickleReceived();
}

// Verifies that subsequent tickles arriving while the opt-in bubble is already
// showing do not trigger duplicate `ShowGmailOtpOptInBubble()` calls until the
// current bubble closes.
TEST_F(GmailOtpOptInCoordinatorTest, DoesNotTriggerWhileBubbleAlreadyShowing) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);
  SimulateOtpFieldDetected();

  base::OnceCallback<void(GmailOtpOptInResult)> bubble_callback;
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble)
      .WillOnce([&](const std::u16string&,
                    base::OnceCallback<void(GmailOtpOptInResult)> callback) {
        bubble_callback = std::move(callback);
      });
  SimulateTickleReceived();
  Mock::VerifyAndClearExpectations(&client_);

  // A second tickle while the bubble is still open is ignored.
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateTickleReceived();
  Mock::VerifyAndClearExpectations(&client_);

  // Once the bubble closes without opting out (e.g. lost focus), a new tickle
  // can trigger the bubble again.
  std::move(bubble_callback).Run(GmailOtpOptInResult::kLostFocus);
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(1);
  SimulateTickleReceived();
}

// Gating check: Do not show during the 90-day cool-off period after the user
// declined the opt-in bubble.
TEST_F(GmailOtpOptInCoordinatorTest,
       SuppressesBubbleDuringDeclinedCooldownPeriod) {
  SignIn();
  prefs::SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      client_.GetPrefs(), base::Time::Now());
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  task_environment_.FastForwardBy(base::Days(89));
  SimulateOtpFieldDetected();
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateTickleReceived();
}

// Gating check: Allow showing the opt-in bubble again once the 90-day cool-off
// period has elapsed.
TEST_F(GmailOtpOptInCoordinatorTest,
       ShowsBubbleAfterDeclinedCooldownPeriodExpires) {
  SignIn();
  prefs::SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      client_.GetPrefs(), base::Time::Now());
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  task_environment_.FastForwardBy(base::Days(90));
  SimulateOtpFieldDetected();
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(1);
  SimulateTickleReceived();
}

// Outcome 1 (`kAccepted`): Enables `kAutofillGmailOtpFillingEnabled` and clears
// any previous dismissal timestamp.
TEST_F(GmailOtpOptInCoordinatorTest,
       OutcomeAccepted_EnablesPrefAndClearsDismissalTimestamp) {
  SignIn();
  // Set an old dismissal timestamp (>90 days ago) so the bubble can show.
  prefs::SetAutofillGmailOtpFillingActivationDismissalTimestamp(
      client_.GetPrefs(), base::Time::Now() - base::Days(100));
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  base::OnceCallback<void(GmailOtpOptInResult)> captured_callback;
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble)
      .WillOnce([&](const std::u16string&,
                    base::OnceCallback<void(GmailOtpOptInResult)> cb) {
        captured_callback = std::move(cb);
      });

  SimulateOtpFieldDetected();
  SimulateTickleReceived();
  ASSERT_TRUE(captured_callback);

  std::move(captured_callback).Run(GmailOtpOptInResult::kAccepted);

  EXPECT_TRUE(prefs::IsAutofillGmailOtpFillingEnabled(client_.GetPrefs()));
  EXPECT_TRUE(prefs::GetAutofillGmailOtpFillingActivationDismissalTimestamp(
                  client_.GetPrefs())
                  .is_null());

  // Subsequent tickles do not show the bubble because the pref is now enabled.
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateTickleReceived();
}

// Outcome 2 (`kDeclined`): Records the dismissal timestamp and suppresses the
// bubble for 90 days.
TEST_F(GmailOtpOptInCoordinatorTest,
       OutcomeDeclined_RecordsDismissalTimestamp) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  base::OnceCallback<void(GmailOtpOptInResult)> captured_callback;
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble)
      .WillOnce([&](const std::u16string&,
                    base::OnceCallback<void(GmailOtpOptInResult)> cb) {
        captured_callback = std::move(cb);
      });

  SimulateOtpFieldDetected();
  SimulateTickleReceived();
  ASSERT_TRUE(captured_callback);

  const base::Time dismissal_time = base::Time::Now();
  std::move(captured_callback).Run(GmailOtpOptInResult::kDeclined);

  EXPECT_FALSE(prefs::IsAutofillGmailOtpFillingEnabled(client_.GetPrefs()));
  EXPECT_EQ(prefs::GetAutofillGmailOtpFillingActivationDismissalTimestamp(
                client_.GetPrefs()),
            dismissal_time);

  // Subsequent tickles within 90 days are suppressed.
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateTickleReceived();
}

// Outcome 3 (`kClosed` via X button or Esc): Suppresses the bubble for the rest
// of the current browser session (even across new coordinator/tab instances)
// without recording a 90-day dismissal timestamp.
TEST_F(GmailOtpOptInCoordinatorTest,
       OutcomeClosed_SuppressesForCurrentSessionOnly) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  base::OnceCallback<void(GmailOtpOptInResult)> captured_callback;
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble)
      .WillOnce([&](const std::u16string&,
                    base::OnceCallback<void(GmailOtpOptInResult)> cb) {
        captured_callback = std::move(cb);
      });

  SimulateOtpFieldDetected();
  SimulateTickleReceived();
  ASSERT_TRUE(captured_callback);

  std::move(captured_callback).Run(GmailOtpOptInResult::kClosed);

  EXPECT_FALSE(prefs::IsAutofillGmailOtpFillingEnabled(client_.GetPrefs()));
  EXPECT_TRUE(prefs::GetAutofillGmailOtpFillingActivationDismissalTimestamp(
                  client_.GetPrefs())
                  .is_null());

  // Subsequent tickles in the same coordinator or a newly created coordinator
  // in the same browser session do not trigger the bubble.
  GmailOtpOptInCoordinator second_tab_coordinator(client_, &gmail_otp_backend_,
                                                  session_user_data_);
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateTickleReceived();
}

// Outcome 4 (`kLostFocus` / `kDiscarded` / `kUnknown`): Does not modify prefs
// or session dismissal state, allowing future triggers.
TEST_F(GmailOtpOptInCoordinatorTest,
       OutcomeLostFocusOrDiscarded_AllowsFutureTrigger) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &gmail_otp_backend_,
                                       session_user_data_);

  SimulateOtpFieldDetected();

  for (GmailOtpOptInResult result :
       {GmailOtpOptInResult::kLostFocus, GmailOtpOptInResult::kDiscarded,
        GmailOtpOptInResult::kUnknown}) {
    base::OnceCallback<void(GmailOtpOptInResult)> captured_callback;
    EXPECT_CALL(client_, ShowGmailOtpOptInBubble)
        .WillOnce([&](const std::u16string&,
                      base::OnceCallback<void(GmailOtpOptInResult)> cb) {
          captured_callback = std::move(cb);
        });

    SimulateTickleReceived();
    ASSERT_TRUE(captured_callback);
    std::move(captured_callback).Run(result);
    Mock::VerifyAndClearExpectations(&client_);

    EXPECT_FALSE(prefs::IsAutofillGmailOtpFillingEnabled(client_.GetPrefs()));
    EXPECT_TRUE(prefs::GetAutofillGmailOtpFillingActivationDismissalTimestamp(
                    client_.GetPrefs())
                    .is_null());
  }
}

}  // namespace
}  // namespace autofill
