// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_coordinator.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/gmail_otp_opt_in_result.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/mock_one_time_token_service.h"
#include "components/one_time_tokens/core/browser/one_time_token_service.h"
#include "components/one_time_tokens/core/browser/util/expiring_subscription_manager.h"
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

class GmailOtpOptInCoordinatorTest : public Test {
 public:
  GmailOtpOptInCoordinatorTest() {
    auto detector = std::make_unique<TestOtpFieldDetector>();
    detector_ = detector.get();
    client_.set_otp_field_detector(std::move(detector));

    ON_CALL(
        mock_ott_service_,
        SubscribeToTickles(one_time_tokens::OneTimeTokenSource::kGmail, _, _))
        .WillByDefault(
            [this](one_time_tokens::OneTimeTokenSource, base::Time exp,
                   one_time_tokens::OneTimeTokenService::TickleCallback cb) {
              return subscription_manager_.Subscribe(
                  exp, std::move(cb),
                  /*expiration_callback=*/base::DoNothing());
            });
  }

  void SignIn(const std::string& email = "user@gmail.com") {
    client_.identity_test_environment().MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
  }

  void SimulateOtpFieldDetected() {
    detector_->AddFormAndNotifyIfNecessary(test::MakeFormGlobalId());
  }

  void SimulateTickleReceived() {
    subscription_manager_.Notify(one_time_tokens::OneTimeTokenSource::kGmail);
  }

 protected:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockOptInAutofillClient> client_;
  raw_ptr<TestOtpFieldDetector> detector_ = nullptr;
  NiceMock<one_time_tokens::MockOneTimeTokenService> mock_ott_service_;
  one_time_tokens::ExpiringSubscriptionManager<void(
      one_time_tokens::OneTimeTokenSource)>
      subscription_manager_;
};

TEST_F(GmailOtpOptInCoordinatorTest, NullServiceDoesNotCrash) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_,
                                       /*one_time_token_service=*/nullptr);
  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateOtpFieldDetected();
}

// Verifies that the coordinator does not subscribe to tickles until an OTP
// field is detected on the page.
TEST_F(GmailOtpOptInCoordinatorTest,
       DoesNotSubscribeToTicklesUntilOtpFieldDetected) {
  SignIn();
  EXPECT_CALL(mock_ott_service_, SubscribeToTickles).Times(0);
  GmailOtpOptInCoordinator coordinator(client_, &mock_ott_service_);
  Mock::VerifyAndClearExpectations(&mock_ott_service_);

  EXPECT_CALL(mock_ott_service_,
              SubscribeToTickles(
                  one_time_tokens::OneTimeTokenSource::kGmail,
                  base::Time::Now() +
                      GmailOtpOptInCoordinator::kTickleSubscriptionDuration,
                  _));
  SimulateOtpFieldDetected();
}

// Scenario A: The OTP form is detected first, and a Gmail tickle arrives later
// within the subscription window.
TEST_F(GmailOtpOptInCoordinatorTest,
       TriggersWhenOtpFieldDetectedBeforeTickleReceived) {
  SignIn("alice@gmail.com");
  GmailOtpOptInCoordinator coordinator(client_, &mock_ott_service_);

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
  GmailOtpOptInCoordinator coordinator(client_, &mock_ott_service_);

  ON_CALL(mock_ott_service_,
          SubscribeToTickles(one_time_tokens::OneTimeTokenSource::kGmail, _, _))
      .WillByDefault(
          [this](one_time_tokens::OneTimeTokenSource, base::Time exp,
                 one_time_tokens::OneTimeTokenService::TickleCallback cb) {
            cb.Run(one_time_tokens::OneTimeTokenSource::kGmail);
            return subscription_manager_.Subscribe(
                exp, std::move(cb),
                /*expiration_callback=*/base::DoNothing());
          });

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

  ON_CALL(mock_ott_service_,
          SubscribeToTickles(one_time_tokens::OneTimeTokenSource::kGmail, _, _))
      .WillByDefault(
          [this](one_time_tokens::OneTimeTokenSource, base::Time exp,
                 one_time_tokens::OneTimeTokenService::TickleCallback cb) {
            cb.Run(one_time_tokens::OneTimeTokenSource::kGmail);
            return subscription_manager_.Subscribe(
                exp, std::move(cb),
                /*expiration_callback=*/base::DoNothing());
          });

  EXPECT_CALL(client_,
              ShowGmailOtpOptInBubble(std::u16string(u"sync@gmail.com"), _));
  GmailOtpOptInCoordinator coordinator(client_, &mock_ott_service_);
}

// Verifies that once `kTickleSubscriptionDuration` elapses after OTP field
// detection, the tickle subscription expires and subsequent tickles do not
// trigger the bubble.
TEST_F(GmailOtpOptInCoordinatorTest,
       DoesNotTriggerAfterTickleSubscriptionExpires) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &mock_ott_service_);

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
  GmailOtpOptInCoordinator coordinator(client_, &mock_ott_service_);

  EXPECT_CALL(mock_ott_service_, SubscribeToTickles).Times(1);
  FormGlobalId form1 = test::MakeFormGlobalId();
  detector_->AddFormAndNotifyIfNecessary(form1);
  Mock::VerifyAndClearExpectations(&mock_ott_service_);

  // Advance 2 minutes, then navigate to a new OTP form (0 -> 1 transition).
  task_environment_.FastForwardBy(base::Minutes(2));
  detector_->RemoveFormAndNotifyIfNecessary(form1);
  EXPECT_CALL(mock_ott_service_, SubscribeToTickles).Times(0);
  detector_->AddFormAndNotifyIfNecessary(test::MakeFormGlobalId());

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
  GmailOtpOptInCoordinator coordinator(client_, &mock_ott_service_);

  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateOtpFieldDetected();
  SimulateTickleReceived();
}

// Gating check: Do not show if there is no signed-in primary account.
TEST_F(GmailOtpOptInCoordinatorTest, DoesNotTriggerWhenNotSignedIn) {
  GmailOtpOptInCoordinator coordinator(client_, &mock_ott_service_);

  EXPECT_CALL(client_, ShowGmailOtpOptInBubble).Times(0);
  SimulateOtpFieldDetected();
  SimulateTickleReceived();
}

// Verifies that subsequent tickles arriving while the opt-in bubble is already
// showing do not trigger duplicate `ShowGmailOtpOptInBubble()` calls until the
// current bubble closes.
TEST_F(GmailOtpOptInCoordinatorTest, DoesNotTriggerWhileBubbleAlreadyShowing) {
  SignIn();
  GmailOtpOptInCoordinator coordinator(client_, &mock_ott_service_);
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

}  // namespace
}  // namespace autofill
