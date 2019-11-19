// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/constants.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/assistant/service_context.h"
#include "chromeos/services/assistant/test_support/fake_assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/test_support/fake_client.h"
#include "chromeos/services/assistant/test_support/fully_initialized_assistant_state.h"
#include "chromeos/services/assistant/test_support/mock_media_manager.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"
#include "services/media_session/public/mojom/media_session.mojom-shared.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

using media_session::mojom::MediaSessionAction;
using testing::StrictMock;
using CommunicationErrorType = AssistantManagerService::CommunicationErrorType;

namespace {
// Return the list of all libassistant error codes that are considered to be
// authentication errors. This list is created on demand as there is no clear
// enum that defines these, and we don't want to hard code this list in the
// test.
static std::vector<int> GetAuthenticationErrorCodes() {
  const int kMinErrorCode = GetLowestErrorCode();
  const int kMaxErrorCode = GetHighestErrorCode();

  std::vector<int> result;
  for (int code = kMinErrorCode; code <= kMaxErrorCode; ++code) {
    if (IsAuthError(code))
      result.push_back(code);
  }

  return result;
}

// Return a list of some libassistant error codes that are not considered to be
// authentication errors.  Note we do not return all such codes as there are
// simply too many and testing them all significantly slows down the tests.
static std::vector<int> GetNonAuthenticationErrorCodes() {
  return {-99999, 0, 1};
}

class FakeAssistantClient : public FakeClient {
 public:
  FakeAssistantClient() = default;

  void RequestBatteryMonitor(
      mojo::PendingReceiver<device::mojom::BatteryMonitor> receiver) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAssistantClient);
};

class FakeServiceContext : public ServiceContext {
 public:
  FakeServiceContext(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      ash::AssistantState* assistant_state,
      PowerManagerClient* power_manager_client)
      : main_task_runner_(main_task_runner),
        assistant_state_(assistant_state),
        power_manager_client_(power_manager_client) {}
  ~FakeServiceContext() override = default;

  ash::mojom::AssistantAlarmTimerController* assistant_alarm_timer_controller()
      override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  mojom::AssistantController* assistant_controller() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  ash::mojom::AssistantNotificationController*
  assistant_notification_controller() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  ash::mojom::AssistantScreenContextController*
  assistant_screen_context_controller() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  ash::AssistantStateBase* assistant_state() override {
    return assistant_state_;
  }

  CrasAudioHandler* cras_audio_handler() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  mojom::DeviceActions* device_actions() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  scoped_refptr<base::SequencedTaskRunner> main_task_runner() override {
    return main_task_runner_;
  }

  PowerManagerClient* power_manager_client() override {
    return power_manager_client_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  ash::AssistantState* const assistant_state_;
  PowerManagerClient* const power_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(FakeServiceContext);
};

class CommunicationErrorObserverMock
    : public AssistantManagerService::CommunicationErrorObserver {
 public:
  CommunicationErrorObserverMock() = default;
  ~CommunicationErrorObserverMock() override = default;

  MOCK_METHOD(void,
              OnCommunicationError,
              (AssistantManagerService::CommunicationErrorType error));

 private:
  DISALLOW_COPY_AND_ASSIGN(CommunicationErrorObserverMock);
};

class StateObserverMock : public AssistantManagerService::StateObserver {
 public:
  StateObserverMock() = default;
  ~StateObserverMock() override = default;

  MOCK_METHOD(void, OnStateChanged, (AssistantManagerService::State new_state));

 private:
  DISALLOW_COPY_AND_ASSIGN(StateObserverMock);
};

class AssistantManagerServiceImplTest : public testing::Test {
 public:
  AssistantManagerServiceImplTest() = default;
  ~AssistantManagerServiceImplTest() override = default;

  void SetUp() override {
    PowerManagerClient::InitializeFake();
    FakePowerManagerClient::Get()->SetTabletMode(
        PowerManagerClient::TabletMode::OFF, base::TimeTicks());

    mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor;
    assistant_client_.RequestBatteryMonitor(
        battery_monitor.InitWithNewPipeAndPassReceiver());

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);

    service_context_ = std::make_unique<FakeServiceContext>(
        task_environment.GetMainThreadTaskRunner(), &assistant_state_,
        PowerManagerClient::Get());

    auto delegate = std::make_unique<FakeAssistantManagerServiceDelegate>();
    delegate_ = delegate.get();

    assistant_manager_service_ = std::make_unique<AssistantManagerServiceImpl>(
        &assistant_client_, service_context_.get(), std::move(delegate),
        shared_url_loader_factory_->Clone(),
        /*is_signed_out_mode=*/false);
  }

  void TearDown() override {
    assistant_manager_service_.reset();
    PowerManagerClient::Shutdown();
  }

  AssistantManagerServiceImpl* assistant_manager_service() {
    return assistant_manager_service_.get();
  }

  ash::AssistantState* assistant_state() { return &assistant_state_; }

  FakeAssistantManager* fake_assistant_manager() {
    return delegate_->assistant_manager();
  }

  FakeAssistantManagerInternal* fake_assistant_manager_internal() {
    return delegate_->assistant_manager_internal();
  }

  void Start() {
    assistant_manager_service()->Start("dummy-access-token",
                                       /*enable_hotword=*/false);
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  // Adds a state observer mock, and add the expectation for the fact that it
  // auto-fires the observer.
  void AddStateObserver(StateObserverMock* observer) {
    EXPECT_CALL(*observer,
                OnStateChanged(assistant_manager_service()->GetState()));
    assistant_manager_service()->AddAndFireStateObserver(observer);
  }

  void WaitUntilStartIsFinished() {
    assistant_manager_service()->WaitUntilStartIsFinishedForTesting();
  }

  // Raise all the |libassistant_error_codes| as communication errors from
  // libassistant, and check that they are reported to our
  // |AssistantCommunicationErrorObserver| as errors of type |expected_type|.
  void TestCommunicationErrors(const std::vector<int>& libassistant_error_codes,
                               CommunicationErrorType expected_error) {
    Start();
    WaitUntilStartIsFinished();

    auto* delegate =
        fake_assistant_manager_internal()->assistant_manager_delegate();

    for (int code : libassistant_error_codes) {
      CommunicationErrorObserverMock observer;
      assistant_manager_service()->AddCommunicationErrorObserver(&observer);

      EXPECT_CALL(observer, OnCommunicationError(expected_error));

      delegate->OnCommunicationError(code);
      RunUntilIdle();

      assistant_manager_service()->RemoveCommunicationErrorObserver(&observer);

      ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer))
          << "Failure for error code " << code;
    }
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;

  FakeAssistantClient assistant_client_{};
  FullyInitializedAssistantState assistant_state_;

  std::unique_ptr<FakeServiceContext> service_context_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  FakeAssistantManagerServiceDelegate* delegate_;

  std::unique_ptr<AssistantManagerServiceImpl> assistant_manager_service_;

  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceImplTest);
};
}  // namespace

TEST_F(AssistantManagerServiceImplTest, StateShouldStartAsStopped) {
  EXPECT_EQ(AssistantManagerService::STOPPED,
            assistant_manager_service()->GetState());
}

TEST_F(AssistantManagerServiceImplTest,
       StateShouldChangeToStartingAfterCallingStart) {
  Start();

  EXPECT_EQ(AssistantManagerService::STARTING,
            assistant_manager_service()->GetState());
}

TEST_F(AssistantManagerServiceImplTest,
       StateShouldRemainStartingUntilLibassistantIsStarted) {
  Start();

  fake_assistant_manager()->BlockStartCalls();
  RunUntilIdle();

  EXPECT_EQ(AssistantManagerService::STARTING,
            assistant_manager_service()->GetState());

  fake_assistant_manager()->UnblockStartCalls();
  WaitUntilStartIsFinished();

  EXPECT_EQ(AssistantManagerService::STARTED,
            assistant_manager_service()->GetState());
}

TEST_F(AssistantManagerServiceImplTest,
       StateShouldBecomeRunningAfterLibassistantSignalsOnStartFinished) {
  Start();
  WaitUntilStartIsFinished();

  fake_assistant_manager()->device_state_listener()->OnStartFinished();

  EXPECT_EQ(AssistantManagerService::RUNNING,
            assistant_manager_service()->GetState());
}

TEST_F(AssistantManagerServiceImplTest, ShouldSetStateToStoppedAfterStopping) {
  Start();
  WaitUntilStartIsFinished();
  ASSERT_EQ(AssistantManagerService::STARTED,
            assistant_manager_service()->GetState());

  assistant_manager_service()->Stop();
  EXPECT_EQ(AssistantManagerService::STOPPED,
            assistant_manager_service()->GetState());
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldReportAuthenticationErrorsToCommunicationErrorObservers) {
  TestCommunicationErrors(GetAuthenticationErrorCodes(),
                          CommunicationErrorType::AuthenticationError);
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldReportNonAuthenticationErrorsToCommunicationErrorObservers) {
  std::vector<int> non_authentication_errors = GetNonAuthenticationErrorCodes();

  // check to ensure these are not authentication errors.
  for (int code : non_authentication_errors)
    ASSERT_FALSE(IsAuthError(code));

  // Run the actual unittest
  TestCommunicationErrors(non_authentication_errors,
                          CommunicationErrorType::Other);
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldNotCrashWhenSettingAuthTokenBeforeStartFinished) {
  Start();

  assistant_manager_service()->SetAccessToken("<this-should-not-crash>");
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldPassAccessTokenToAssistantManager) {
  Start();
  WaitUntilStartIsFinished();

  assistant_manager_service()->SetAccessToken("<the-access-token>");

  EXPECT_EQ("<the-access-token>", fake_assistant_manager()->access_token());
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldPassDefaultUserIdToAssistantManagerWhenSettingAccessToken) {
  Start();
  WaitUntilStartIsFinished();

  assistant_manager_service()->SetAccessToken("<the-access-token>");

  EXPECT_EQ(kUserID, fake_assistant_manager()->user_id());
}

TEST_F(AssistantManagerServiceImplTest, ShouldPauseMediaManagerOnPause) {
  StrictMock<MockMediaManager> mock;
  fake_assistant_manager()->SetMediaManager(&mock);

  Start();
  WaitUntilStartIsFinished();

  EXPECT_CALL(mock, Pause);

  assistant_manager_service()->UpdateInternalMediaPlayerStatus(
      MediaSessionAction::kPause);
}

TEST_F(AssistantManagerServiceImplTest, ShouldResumeMediaManagerOnPlay) {
  StrictMock<MockMediaManager> mock;
  fake_assistant_manager()->SetMediaManager(&mock);

  Start();
  WaitUntilStartIsFinished();

  EXPECT_CALL(mock, Resume);

  assistant_manager_service()->UpdateInternalMediaPlayerStatus(
      MediaSessionAction::kPlay);
}

TEST_F(AssistantManagerServiceImplTest, ShouldIgnoreOtherMediaManagerActions) {
  const auto unsupported_media_session_actions = {
      MediaSessionAction::kPreviousTrack, MediaSessionAction::kNextTrack,
      MediaSessionAction::kSeekBackward,  MediaSessionAction::kSeekForward,
      MediaSessionAction::kSkipAd,        MediaSessionAction::kStop,
      MediaSessionAction::kSeekTo,        MediaSessionAction::kScrubTo,
  };

  StrictMock<MockMediaManager> mock;
  fake_assistant_manager()->SetMediaManager(&mock);

  Start();
  WaitUntilStartIsFinished();

  for (auto action : unsupported_media_session_actions) {
    // If this is not ignored, |mock| will complain about an uninterested call.
    assistant_manager_service()->UpdateInternalMediaPlayerStatus(action);
  }
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldNotCrashWhenMediaManagerIsAbsent) {
  Start();
  WaitUntilStartIsFinished();

  assistant_manager_service()->UpdateInternalMediaPlayerStatus(
      media_session::mojom::MediaSessionAction::kPlay);
}

TEST_F(AssistantManagerServiceImplTest, ShouldFireStateObserverWhenAddingIt) {
  StrictMock<StateObserverMock> observer;
  EXPECT_CALL(observer,
              OnStateChanged(AssistantManagerService::State::STOPPED));

  assistant_manager_service()->AddAndFireStateObserver(&observer);

  assistant_manager_service()->RemoveStateObserver(&observer);
}

TEST_F(AssistantManagerServiceImplTest, ShouldFireStateObserverWhenStarting) {
  StrictMock<StateObserverMock> observer;
  AddStateObserver(&observer);

  fake_assistant_manager()->BlockStartCalls();

  EXPECT_CALL(observer,
              OnStateChanged(AssistantManagerService::State::STARTING));
  Start();

  assistant_manager_service()->RemoveStateObserver(&observer);
  fake_assistant_manager()->UnblockStartCalls();
}

TEST_F(AssistantManagerServiceImplTest, ShouldFireStateObserverWhenStarted) {
  StrictMock<StateObserverMock> observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer,
              OnStateChanged(AssistantManagerService::State::STARTING));
  EXPECT_CALL(observer,
              OnStateChanged(AssistantManagerService::State::STARTED));
  Start();
  WaitUntilStartIsFinished();

  assistant_manager_service()->RemoveStateObserver(&observer);
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldFireStateObserverWhenLibAssistantSignalsOnStartFinished) {
  Start();
  WaitUntilStartIsFinished();

  StrictMock<StateObserverMock> observer;
  AddStateObserver(&observer);
  EXPECT_CALL(observer,
              OnStateChanged(AssistantManagerService::State::RUNNING));

  fake_assistant_manager()->device_state_listener()->OnStartFinished();

  assistant_manager_service()->RemoveStateObserver(&observer);
}

TEST_F(AssistantManagerServiceImplTest, ShouldFireStateObserverWhenStopping) {
  Start();
  WaitUntilStartIsFinished();

  StrictMock<StateObserverMock> observer;
  AddStateObserver(&observer);
  EXPECT_CALL(observer,
              OnStateChanged(AssistantManagerService::State::STOPPED));

  assistant_manager_service()->Stop();

  assistant_manager_service()->RemoveStateObserver(&observer);
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldNotFireStateObserverAfterItIsRemoved) {
  StrictMock<StateObserverMock> observer;
  AddStateObserver(&observer);

  assistant_manager_service()->RemoveStateObserver(&observer);
  EXPECT_CALL(observer, OnStateChanged).Times(0);

  Start();
}

}  // namespace assistant
}  // namespace chromeos
