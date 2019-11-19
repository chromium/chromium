// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/service.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/services/assistant/assistant_state_proxy.h"
#include "chromeos/services/assistant/fake_assistant_manager_service_impl.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/test_support/fake_client.h"
#include "chromeos/services/assistant/test_support/fully_initialized_assistant_state.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

namespace {
constexpr base::TimeDelta kDefaultTokenExpirationDelay =
    base::TimeDelta::FromMilliseconds(1000);
}  // namespace

class FakeIdentityAccessor : identity::mojom::IdentityAccessor {
 public:
  FakeIdentityAccessor()
      : access_token_expriation_delay_(kDefaultTokenExpirationDelay) {}

  mojo::PendingRemote<identity::mojom::IdentityAccessor>
  CreatePendingRemoteAndBind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void SetAccessTokenExpirationDelay(base::TimeDelta delay) {
    access_token_expriation_delay_ = delay;
  }

  void SetShouldFail(bool fail) { should_fail_ = fail; }

  int get_access_token_count() const { return get_access_token_count_; }

 private:
  // identity::mojom::IdentityAccessor:
  void GetPrimaryAccountInfo(GetPrimaryAccountInfoCallback callback) override {
    CoreAccountId account_id("account_id");
    std::string gaia = "fakegaiaid";
    std::string email = "fake@email";

    identity::AccountState account_state;
    account_state.has_refresh_token = true;
    account_state.is_primary_account = true;

    std::move(callback).Run(account_id, gaia, email, account_state);
  }

  void GetPrimaryAccountWhenAvailable(
      GetPrimaryAccountWhenAvailableCallback callback) override {}
  void GetAccessToken(const CoreAccountId& account_id,
                      const ::identity::ScopeSet& scopes,
                      const std::string& consumer_id,
                      GetAccessTokenCallback callback) override {
    GoogleServiceAuthError auth_error(
        should_fail_ ? GoogleServiceAuthError::CONNECTION_FAILED
                     : GoogleServiceAuthError::NONE);
    std::move(callback).Run(
        should_fail_ ? base::nullopt
                     : base::Optional<std::string>("fake access token"),
        base::Time::Now() + access_token_expriation_delay_, auth_error);
    ++get_access_token_count_;
  }

  mojo::Receiver<identity::mojom::IdentityAccessor> receiver_{this};

  base::TimeDelta access_token_expriation_delay_;

  int get_access_token_count_ = 0;

  bool should_fail_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeIdentityAccessor);
};

class FakeAssistantClient : public FakeClient {
 public:
  explicit FakeAssistantClient(ash::AssistantState* assistant_state)
      : assistant_state_(assistant_state),
        status_(ash::mojom::AssistantState::NOT_READY) {}

  ash::mojom::AssistantState status() { return status_; }

 private:
  // FakeClient:

  void OnAssistantStatusChanged(ash::mojom::AssistantState new_state) override {
    status_ = new_state;
  }

  void RequestAssistantStateController(
      mojo::PendingReceiver<ash::mojom::AssistantStateController> receiver)
      override {
    assistant_state_->BindReceiver(std::move(receiver));
  }

  ash::AssistantState* const assistant_state_;
  ash::mojom::AssistantState status_;

  DISALLOW_COPY_AND_ASSIGN(FakeAssistantClient);
};

class FakeDeviceActions : mojom::DeviceActions {
 public:
  FakeDeviceActions() {}

  mojo::PendingRemote<mojom::DeviceActions> CreatePendingRemoteAndBind() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  // mojom::DeviceActions:
  void SetWifiEnabled(bool enabled) override {}
  void SetBluetoothEnabled(bool enabled) override {}
  void GetScreenBrightnessLevel(
      GetScreenBrightnessLevelCallback callback) override {
    std::move(callback).Run(true, 1.0);
  }
  void SetScreenBrightnessLevel(double level, bool gradual) override {}
  void SetNightLightEnabled(bool enabled) override {}
  void OpenAndroidApp(chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
                      OpenAndroidAppCallback callback) override {}
  void VerifyAndroidApp(
      std::vector<chromeos::assistant::mojom::AndroidAppInfoPtr> apps_info,
      VerifyAndroidAppCallback callback) override {}
  void LaunchAndroidIntent(const std::string& intent) override {}
  void AddAppListEventSubscriber(
      mojo::PendingRemote<chromeos::assistant::mojom::AppListEventSubscriber>
          subscriber) override {}

  mojo::Receiver<mojom::DeviceActions> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceActions);
};

class AssistantServiceTest : public testing::Test {
 public:
  AssistantServiceTest() = default;

  ~AssistantServiceTest() override = default;

  void SetUp() override {
    chromeos::CrasAudioHandler::InitializeForTesting();

    PowerManagerClient::InitializeFake();
    FakePowerManagerClient::Get()->SetTabletMode(
        PowerManagerClient::TabletMode::OFF, base::TimeTicks());

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);

    prefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.SetBoolean(prefs::kAssistantEnabled, true);
    pref_service_.SetBoolean(prefs::kAssistantHotwordEnabled, true);

    service_ = std::make_unique<Service>(
        remote_service_.BindNewPipeAndPassReceiver(),
        shared_url_loader_factory_->Clone(), &pref_service_);
    service_->is_test_ = true;

    mock_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto mock_timer = std::make_unique<base::OneShotTimer>(
        mock_task_runner_->GetMockTickClock());
    mock_timer->SetTaskRunner(mock_task_runner_);
    service_->SetTimerForTesting(std::move(mock_timer));

    service_->SetIdentityAccessorForTesting(
        fake_identity_accessor_.CreatePendingRemoteAndBind());

    remote_service_->Init(fake_assistant_client_.MakeRemote(),
                          fake_device_actions_.CreatePendingRemoteAndBind(),
                          /*is_test=*/true);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    service_.reset();
    PowerManagerClient::Shutdown();
    chromeos::CrasAudioHandler::Shutdown();
  }

  Service* service() { return service_.get(); }

  FakeAssistantManagerServiceImpl* assistant_manager() {
    auto* result = static_cast<FakeAssistantManagerServiceImpl*>(
        service_->assistant_manager_service_.get());
    DCHECK(result);
    return result;
  }

  FakeIdentityAccessor* identity_accessor() { return &fake_identity_accessor_; }

  ash::AssistantState* assistant_state() { return &assistant_state_; }

  PrefService* pref_service() { return &pref_service_; }

  FakeAssistantClient* client() { return &fake_assistant_client_; }

  base::TestMockTimeTaskRunner* mock_task_runner() {
    return mock_task_runner_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<Service> service_;
  mojo::Remote<mojom::AssistantService> remote_service_;

  FullyInitializedAssistantState assistant_state_;

  FakeIdentityAccessor fake_identity_accessor_;
  FakeAssistantClient fake_assistant_client_{&assistant_state_};
  FakeDeviceActions fake_device_actions_;

  TestingPrefServiceSimple pref_service_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  std::unique_ptr<base::OneShotTimer> mock_timer_;

  DISALLOW_COPY_AND_ASSIGN(AssistantServiceTest);
};

TEST_F(AssistantServiceTest, RefreshTokenAfterExpire) {
  auto current_count = identity_accessor()->get_access_token_count();
  mock_task_runner()->FastForwardBy(kDefaultTokenExpirationDelay / 2);
  base::RunLoop().RunUntilIdle();

  // Before token expire, should not request new token.
  EXPECT_EQ(identity_accessor()->get_access_token_count(), current_count);

  mock_task_runner()->FastForwardBy(kDefaultTokenExpirationDelay);
  base::RunLoop().RunUntilIdle();

  // After token expire, should request once.
  EXPECT_EQ(identity_accessor()->get_access_token_count(), ++current_count);
}

TEST_F(AssistantServiceTest, RetryRefreshTokenAfterFailure) {
  auto current_count = identity_accessor()->get_access_token_count();
  identity_accessor()->SetShouldFail(true);
  mock_task_runner()->FastForwardBy(kDefaultTokenExpirationDelay);
  base::RunLoop().RunUntilIdle();

  // Token request failed.
  EXPECT_EQ(identity_accessor()->get_access_token_count(), ++current_count);

  base::RunLoop().RunUntilIdle();

  // Token request automatically retry.
  identity_accessor()->SetShouldFail(false);
  // The failure delay has jitter so fast forward a bit more.
  mock_task_runner()->FastForwardBy(kDefaultTokenExpirationDelay * 2);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(identity_accessor()->get_access_token_count(), ++current_count);
}

TEST_F(AssistantServiceTest, RetryRefreshTokenAfterDeviceWakeup) {
  auto current_count = identity_accessor()->get_access_token_count();
  FakePowerManagerClient::Get()->SendSuspendDone();
  base::RunLoop().RunUntilIdle();

  // Token requested immediately after suspend done.
  EXPECT_EQ(identity_accessor()->get_access_token_count(), ++current_count);
}

TEST_F(AssistantServiceTest, StopImmediatelyIfAssistantIsRunning) {
  // Test is set up as |State::STARTED|.
  assistant_manager()->FinishStart();

  EXPECT_EQ(assistant_manager()->GetState(),
            AssistantManagerService::State::RUNNING);

  pref_service()->SetBoolean(prefs::kAssistantEnabled, false);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(assistant_manager()->GetState(),
            AssistantManagerService::State::STOPPED);
}

TEST_F(AssistantServiceTest, StopDelayedIfAssistantNotFinishedStarting) {
  // Test is set up as |State::STARTING|, turning settings off will trigger
  // logic to try to stop it.
  pref_service()->SetBoolean(prefs::kAssistantEnabled, false);

  EXPECT_EQ(assistant_manager()->GetState(),
            AssistantManagerService::State::STARTING);

  mock_task_runner()->FastForwardBy(kUpdateAssistantManagerDelay);
  base::RunLoop().RunUntilIdle();

  // No change of state because it is still starting.
  EXPECT_EQ(assistant_manager()->GetState(),
            AssistantManagerService::State::STARTING);

  assistant_manager()->FinishStart();

  mock_task_runner()->FastForwardBy(kUpdateAssistantManagerDelay);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(assistant_manager()->GetState(),
            AssistantManagerService::State::STOPPED);
}

TEST_F(AssistantServiceTest, ShouldSetClientStatusToNotReadyWhenStarting) {
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::STARTING);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client()->status(), ash::mojom::AssistantState::NOT_READY);
}

TEST_F(AssistantServiceTest, ShouldSetClientStatusToReadyWhenStarted) {
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::STARTED);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client()->status(), ash::mojom::AssistantState::READY);
}

TEST_F(AssistantServiceTest, ShouldSetClientStatusToNewReadyWhenRunning) {
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::RUNNING);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client()->status(), ash::mojom::AssistantState::NEW_READY);
}

TEST_F(AssistantServiceTest, ShouldSetClientStatusToNotReadyWhenStopped) {
  assistant_manager()->SetStateAndInformObservers(
      AssistantManagerService::State::RUNNING);
  base::RunLoop().RunUntilIdle();

  pref_service()->SetBoolean(prefs::kAssistantEnabled, false);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client()->status(), ash::mojom::AssistantState::NOT_READY);
}

}  // namespace assistant
}  // namespace chromeos
