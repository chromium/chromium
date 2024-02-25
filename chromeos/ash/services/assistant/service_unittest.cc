// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/service.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/assistant/test_support/mock_assistant_controller.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/test_support/fake_assistant_manager_service_impl.h"
#include "chromeos/ash/services/assistant/test_support/fully_initialized_assistant_state.h"
#include "chromeos/ash/services/assistant/test_support/scoped_assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/test_support/scoped_device_actions.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::assistant {

namespace {

constexpr base::TimeDelta kDefaultTokenExpirationDelay =
    base::Milliseconds(60000);

constexpr base::TimeDelta kAutoRecoverTime = base::Seconds(60);

#define EXPECT_STATE(_state) EXPECT_EQ(_state, assistant_manager()->GetState())

const char* kAccessToken = "fake access token";
const char* kGaiaId = "gaia_id_for_user_gmail.com";
const char* kEmailAddress = "user@gmail.com";

// Should be the same value as the one in service.cc.
constexpr int kMaxStartServiceRetries = 1;

}  // namespace

class ScopedFakeAssistantBrowserDelegate
    : public ScopedAssistantBrowserDelegate {
 public:
  explicit ScopedFakeAssistantBrowserDelegate(AssistantState* assistant_state)
      : status_(AssistantStatus::NOT_READY) {}

  AssistantStatus status() { return status_; }

 private:
  // ScopedAssistantBrowserDelegate:
  void OnAssistantStatusChanged(AssistantStatus new_status) override {
    status_ = new_status;
  }

  AssistantStatus status_;
};

class AssistantServiceTest : public testing::Test {
 public:
  AssistantServiceTest() = default;
  AssistantServiceTest(const AssistantServiceTest&) = delete;
  AssistantServiceTest& operator=(const AssistantServiceTest&) = delete;
  ~AssistantServiceTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->SetTabletMode(
        chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);

    prefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.SetBoolean(prefs::kAssistantEnabled, true);
    pref_service_.SetBoolean(prefs::kAssistantHotwordEnabled, true);

    assistant_state_.RegisterPrefChanges(&pref_service_);

    // In production the primary account is set before the service is created.
    identity_test_env_.MakePrimaryAccountAvailable(
        kEmailAddress, signin::ConsentLevel::kSignin);

    service_ = std::make_unique<Service>(shared_url_loader_factory_->Clone(),
                                         identity_test_env_.identity_manager(),
                                         pref_service());
    service_->SetAssistantManagerServiceForTesting(
        std::make_unique<FakeAssistantManagerServiceImpl>());
    service_->SetAutoRecoverTimeForTesting(kAutoRecoverTime);

    service_->Init();
    // Wait for AssistantManagerService to be set.
    base::RunLoop().RunUntilIdle();

    IssueAccessToken(kAccessToken);
    // Simulate that the DLC library is loaded.
    service_->OnLibassistantLoaded(/*success=*/true);
  }

  void TearDown() override {
    service_.reset();
    chromeos::PowerManagerClient::Shutdown();
    CrasAudioHandler::Shutdown();
  }

  void StartAssistantAndWait() {
    pref_service()->SetBoolean(prefs::kAssistantEnabled, true);
    base::RunLoop().RunUntilIdle();
  }

  void StopAssistantAndWait() {
    pref_service()->SetBoolean(prefs::kAssistantEnabled, false);
    base::RunLoop().RunUntilIdle();
  }

  void IssueAccessToken(const std::string& access_token) {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        access_token, base::Time::Now() + kDefaultTokenExpirationDelay);
  }

  Service* service() { return service_.get(); }

  FakeAssistantManagerServiceImpl* assistant_manager() {
    auto* result = static_cast<FakeAssistantManagerServiceImpl*>(
        service_->assistant_manager_service_.get());
    DCHECK(result);
    return result;
  }

  void ResetFakeAssistantManager() {
    assistant_manager()->SetUser(std::nullopt);
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  PrefService* pref_service() { return &pref_service_; }

  AssistantState* assistant_state() { return &assistant_state_; }

  ScopedFakeAssistantBrowserDelegate* client() { return &fake_delegate_; }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  net::BackoffEntry* GetRestartServiceBackoff() {
    return &service_->start_service_retry_backoff_;
  }

  void DecreaseStartServiceBackoff() {
    service_->DecreaseStartServiceBackoff();
  }

  int GetNumberOfFailuresSinceLastServiceRun() {
    return pref_service()->GetInteger(
        prefs::kAssistantNumFailuresSinceLastServiceRun);
  }

  void SetNumberOfFailuresSinceLastServiceRun(int number) {
    pref_service()->SetInteger(prefs::kAssistantNumFailuresSinceLastServiceRun,
                               number);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingPrefServiceSimple pref_service_;

  std::unique_ptr<Service> service_;

  ScopedCrasAudioHandlerForTesting cras_audio_handler_;
  FullyInitializedAssistantState assistant_state_;
  signin::IdentityTestEnvironment identity_test_env_;
  ScopedFakeAssistantBrowserDelegate fake_delegate_{&assistant_state_};
  ScopedDeviceActions fake_device_actions_;
  testing::NiceMock<MockAssistantController> mock_assistant_controller;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(AssistantServiceTest, RefreshTokenAfterExpire) {
  ASSERT_FALSE(identity_test_env()->IsAccessTokenRequestPending());
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay / 2);

  // Before token expire, should not request new token.
  EXPECT_FALSE(identity_test_env()->IsAccessTokenRequestPending());

  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay);

  // After token expire, should request once.
  EXPECT_TRUE(identity_test_env()->IsAccessTokenRequestPending());
}

TEST_F(AssistantServiceTest, RetryRefreshTokenAfterFailure) {
  ASSERT_FALSE(identity_test_env()->IsAccessTokenRequestPending());

  // Let the first token expire. Another will be requested.
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay);
  EXPECT_TRUE(identity_test_env()->IsAccessTokenRequestPending());

  // Reply with an error.
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));
  EXPECT_FALSE(identity_test_env()->IsAccessTokenRequestPending());

  // Token request automatically retry.
  // The failure delay has jitter so fast forward a bit more, but before
  // the returned token would expire again.
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay / 2);

  EXPECT_TRUE(identity_test_env()->IsAccessTokenRequestPending());
}

TEST_F(AssistantServiceTest, RetryRefreshTokenAfterDeviceWakeup) {
  ASSERT_FALSE(identity_test_env()->IsAccessTokenRequestPending());

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
  // Token requested immediately after suspend done.
  EXPECT_TRUE(identity_test_env()->IsAccessTokenRequestPending());
}

TEST_F(AssistantServiceTest, StopImmediatelyIfAssistantIsRunning) {
  // Test is set up as |State::STARTED|.
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);

  StopAssistantAndWait();

  EXPECT_STATE(AssistantManagerService::State::STOPPED);
}

TEST_F(AssistantServiceTest, StopDelayedIfAssistantNotFinishedStarting) {
  EXPECT_STATE(AssistantManagerService::State::STARTING);

  // Turning settings off will trigger logic to try to stop it.
  StopAssistantAndWait();

  EXPECT_STATE(AssistantManagerService::State::STARTING);

  task_environment()->FastForwardBy(kUpdateAssistantManagerDelay);

  // No change of state because it is still starting.
  EXPECT_STATE(AssistantManagerService::State::STARTING);

  assistant_manager()->FinishStart();

  task_environment()->FastForwardBy(kUpdateAssistantManagerDelay);

  EXPECT_STATE(AssistantManagerService::State::STOPPED);
}

TEST_F(AssistantServiceTest, ShouldSendUserInfoWhenStarting) {
  // First stop the service and reset the AssistantManagerService
  assistant_manager()->FinishStart();
  StopAssistantAndWait();
  ResetFakeAssistantManager();

  // Now start the service
  StartAssistantAndWait();

  ASSERT_TRUE(assistant_manager()->access_token().has_value());
  EXPECT_EQ(kAccessToken, assistant_manager()->access_token().value());
  ASSERT_TRUE(assistant_manager()->gaia_id().has_value());
  EXPECT_EQ(kGaiaId, assistant_manager()->gaia_id());
}

TEST_F(AssistantServiceTest, ShouldSendUserInfoWhenAccessTokenIsRefreshed) {
  assistant_manager()->FinishStart();

  // Reset the AssistantManagerService so it forgets the user info sent when
  // starting the service.
  ResetFakeAssistantManager();

  // Now force an access token refresh
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay);
  IssueAccessToken("new token");

  ASSERT_TRUE(assistant_manager()->access_token().has_value());
  EXPECT_EQ("new token", assistant_manager()->access_token());
  ASSERT_TRUE(assistant_manager()->gaia_id().has_value());
  EXPECT_EQ(kGaiaId, assistant_manager()->gaia_id());
}

TEST_F(AssistantServiceTest, ShouldSetClientStatusToNotReadyWhenStarting) {
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::STARTING);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client()->status(), AssistantStatus::NOT_READY);
}

TEST_F(AssistantServiceTest, ShouldKeepClientStatusNotReadyWhenStarted) {
  // Note: even though we've started, we are not ready to handle the queries
  // until LibAssistant tells us we are.
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::STARTED);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client()->status(), AssistantStatus::NOT_READY);
}

TEST_F(AssistantServiceTest, ShouldSetClientStatusToNewReadyWhenRunning) {
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::RUNNING);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client()->status(), AssistantStatus::READY);
}

TEST_F(AssistantServiceTest, ShouldSetClientStatusToNotReadyWhenStopped) {
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::RUNNING);
  base::RunLoop().RunUntilIdle();

  StopAssistantAndWait();

  EXPECT_EQ(client()->status(), AssistantStatus::NOT_READY);
}

TEST_F(AssistantServiceTest, StopImmediatelyIfAssistantIsDisconnected) {
  // Test is set up as |State::STARTED|.
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(client()->status(), AssistantStatus::NOT_READY);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::STARTING);
}

TEST_F(AssistantServiceTest,
       IncreaseBackoffIfAssistantIsDisconnectedAfterStarting) {
  StartAssistantAndWait();
  EXPECT_STATE(AssistantManagerService::State::STARTING);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 1);
}

TEST_F(AssistantServiceTest,
       IncreaseBackoffIfAssistantIsDisconnectedAfterStarted) {
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::STARTED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 1);
}

TEST_F(AssistantServiceTest,
       IncreaseBackoffIfAssistantIsDisconnectedAfterRunning) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 1);
}

TEST_F(AssistantServiceTest, WillRetryIfAssistantIsDisconnectedAfterRunning) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 1);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::STARTING);
}

TEST_F(AssistantServiceTest,
       WillNotRetryIfAssistantIsDisconnectedAfterRunning) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);

  // Will retry start for the first `kMaxStartServiceRetries` times.
  for (int i = 1; i <= kMaxStartServiceRetries; ++i) {
    assistant_manager()->Disconnected();
    EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
    EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), i);

    task_environment()->FastForwardBy(
        GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
    EXPECT_STATE(AssistantManagerService::State::STARTING);
  }

  // Will not retry start after disconnected `kMaxStartServiceRetries` times.
  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries + 1);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
}

TEST_F(AssistantServiceTest, DecreaseBackoff) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);

  for (int i = 1; i <= kMaxStartServiceRetries; ++i) {
    GetRestartServiceBackoff()->InformOfRequest(/*succeeded=*/false);
  }
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries + 1);

  for (int i = kMaxStartServiceRetries; i >= 0; --i) {
    task_environment()->FastForwardBy(kAutoRecoverTime);
    EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), i);
  }

  // The `failure_count` will not be less than 0.
  task_environment()->FastForwardBy(kAutoRecoverTime);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);
}

TEST_F(AssistantServiceTest, WillRetryAfterDecreaseBackoff) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);

  for (int i = 1; i <= kMaxStartServiceRetries; ++i) {
    GetRestartServiceBackoff()->InformOfRequest(/*succeeded=*/false);
  }
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries + 1);

  task_environment()->FastForwardBy(kAutoRecoverTime);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::STARTING);
}

TEST_F(AssistantServiceTest, NoOpWhenRetryStartAfterDecreaseBackoff) {
  for (int i = 1; i <= kMaxStartServiceRetries; ++i) {
    GetRestartServiceBackoff()->InformOfRequest(/*succeeded=*/false);
  }
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries);

  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);

  DecreaseStartServiceBackoff();
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries - 1);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
}

TEST_F(AssistantServiceTest, ResetBackoffAfterReEnableSettings) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);

  for (int i = 1; i <= kMaxStartServiceRetries; ++i) {
    GetRestartServiceBackoff()->InformOfRequest(/*succeeded=*/false);
  }
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries + 1);

  StopAssistantAndWait();
  StartAssistantAndWait();
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);
}

TEST_F(AssistantServiceTest, WillStartAfterReEnableSettings) {
  for (int i = 1; i <= kMaxStartServiceRetries + 1; ++i) {
    GetRestartServiceBackoff()->InformOfRequest(/*succeeded=*/false);
  }
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries + 1);

  StopAssistantAndWait();
  StartAssistantAndWait();
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);
  EXPECT_STATE(AssistantManagerService::State::STARTING);
}

TEST_F(AssistantServiceTest, WillNotStartAfterMaxRetry_OnTokenRefreshed) {
  ResetFakeAssistantManager();
  // Now force an access token refresh.
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay);
  IssueAccessToken("new token");
  EXPECT_STATE(AssistantManagerService::State::STARTING);

  // Now force an access token refresh.
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay);
  for (int i = 1; i <= kMaxStartServiceRetries; ++i) {
    GetRestartServiceBackoff()->InformOfRequest(/*succeeded=*/false);
  }
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries + 1);

  IssueAccessToken("new token");
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
}

TEST_F(AssistantServiceTest,
       IncreaseFailuresPrefIfAssistantIsDisconnectedAfterStarting) {
  StartAssistantAndWait();
  EXPECT_STATE(AssistantManagerService::State::STARTING);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 0);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 1);
}

TEST_F(AssistantServiceTest,
       IncreaseFailuresPrefIfAssistantIsDisconnectedAfterStarted) {
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::STARTED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 0);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 1);
}

TEST_F(AssistantServiceTest,
       IncreaseFailuresPrefIfAssistantIsDisconnectedAfterRunning) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 0);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 1);
}

TEST_F(AssistantServiceTest, ShouldRetryBasedOnNumberOfFailures) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 0);

  // Set pref kNumFailuresSinceLastServiceRun to `kMaxStartServiceRetries - 1`,
  // disconnect will retry.
  SetNumberOfFailuresSinceLastServiceRun(kMaxStartServiceRetries - 1);
  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 1);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), kMaxStartServiceRetries);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::STARTING);

  // Pref kNumFailuresSinceLastServiceRun is kMaxStartServiceRetries now,
  // disconnect will not retry.
  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 2);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(),
            kMaxStartServiceRetries + 1);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
}

TEST_F(AssistantServiceTest,
       DecreaseBackoffRetryWillNotBasedOnNumberOfFailures) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 0);

  for (int i = 1; i <= kMaxStartServiceRetries; ++i) {
    GetRestartServiceBackoff()->InformOfRequest(/*succeeded=*/false);
  }
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries + 1);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 1);

  // Decreasing backoff will retry.
  task_environment()->FastForwardBy(kAutoRecoverTime);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::STARTING);

  // Set pref kNumFailuresSinceLastServiceRun to > `kMaxStartServiceRetries`,
  // decreasing backoff still will retry.
  SetNumberOfFailuresSinceLastServiceRun(kMaxStartServiceRetries + 1);
  task_environment()->FastForwardBy(kAutoRecoverTime);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(),
            kMaxStartServiceRetries - 1);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(),
            kMaxStartServiceRetries + 1);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::STARTING);
}

TEST_F(AssistantServiceTest,
       WillNotResetNumberOfFailuresAfterReEnableSettings) {
  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 0);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 1);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 1);

  StopAssistantAndWait();
  StartAssistantAndWait();
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 1);
}

TEST_F(AssistantServiceTest,
       WillStartAfterReEnableSettingsWithMaxNumberOfFailures) {
  SetNumberOfFailuresSinceLastServiceRun(kMaxStartServiceRetries + 1);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(),
            kMaxStartServiceRetries + 1);

  StopAssistantAndWait();
  StartAssistantAndWait();
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(),
            kMaxStartServiceRetries + 1);
  EXPECT_STATE(AssistantManagerService::State::STARTING);
}

TEST_F(AssistantServiceTest, ResetNumberOfFailuresAfterRunning) {
  SetNumberOfFailuresSinceLastServiceRun(kMaxStartServiceRetries + 1);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(),
            kMaxStartServiceRetries + 1);

  assistant_manager()->FinishStart();
  EXPECT_STATE(AssistantManagerService::State::RUNNING);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(), 0);
}

TEST_F(AssistantServiceTest,
       WillStartAfterMaxNumberOfFailures_OnTokenRefreshed) {
  ResetFakeAssistantManager();
  // Now force an access token refresh.
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay);
  IssueAccessToken("new token");
  EXPECT_STATE(AssistantManagerService::State::STARTING);

  // Now force an access token refresh.
  task_environment()->FastForwardBy(kDefaultTokenExpirationDelay);
  GetRestartServiceBackoff()->InformOfRequest(/*succeeded=*/false);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 1);
  SetNumberOfFailuresSinceLastServiceRun(kMaxStartServiceRetries + 1);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(),
            kMaxStartServiceRetries + 1);

  assistant_manager()->Disconnected();
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 2);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(),
            kMaxStartServiceRetries + 2);

  IssueAccessToken("new token");
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);

  // First decreasing backoff will not restart service.
  task_environment()->FastForwardBy(kAutoRecoverTime);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 1);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(),
            kMaxStartServiceRetries + 2);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::DISCONNECTED);

  // Second decreasing backoff will not restart service.
  task_environment()->FastForwardBy(kAutoRecoverTime);
  EXPECT_EQ(GetRestartServiceBackoff()->failure_count(), 0);
  EXPECT_EQ(GetNumberOfFailuresSinceLastServiceRun(),
            kMaxStartServiceRetries + 2);

  task_environment()->FastForwardBy(
      GetRestartServiceBackoff()->GetTimeUntilRelease() * 1.2);
  EXPECT_STATE(AssistantManagerService::State::STARTING);
}

}  // namespace ash::assistant
