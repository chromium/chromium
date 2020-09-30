// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <utility>

#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"
#include "base/json/json_reader.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/test_support/fake_alarm_timer_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/service_context.h"
#include "chromeos/services/assistant/test_support/fake_assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/test_support/fake_service_context.h"
#include "chromeos/services/assistant/test_support/fully_initialized_assistant_state.h"
#include "chromeos/services/assistant/test_support/mock_assistant_interaction_subscriber.h"
#include "chromeos/services/assistant/test_support/mock_media_manager.h"
#include "chromeos/services/assistant/test_support/scoped_assistant_client.h"
#include "chromeos/services/assistant/test_support/scoped_device_actions.h"
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
using testing::ElementsAre;
using testing::Invoke;
using testing::NiceMock;
using testing::StrictMock;
using CommunicationErrorType = AssistantManagerService::CommunicationErrorType;
using UserInfo = AssistantManagerService::UserInfo;

namespace {

const char* kNoValue = FakeAssistantManager::kNoValue;

#define EXPECT_STATE(_state) \
  EXPECT_EQ(_state, assistant_manager_service()->GetState());

// Adds an AlarmTimerEvent of the given |type| to |events|.
static void AddAlarmTimerEvent(
    std::vector<assistant_client::AlarmTimerEvent>* events,
    assistant_client::AlarmTimerEvent::Type type) {
  events->push_back(assistant_client::AlarmTimerEvent());
  events->back().type = type;
}

// Adds an AlarmTimerEvent of type TIMER with the given |state| to |events|.
static void AddTimerEvent(
    std::vector<assistant_client::AlarmTimerEvent>* events,
    assistant_client::Timer::State state) {
  AddAlarmTimerEvent(events, assistant_client::AlarmTimerEvent::TIMER);
  events->back().timer_data.state = state;
}

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

class AssistantAlarmTimerControllerMock
    : public ash::AssistantAlarmTimerController {
 public:
  AssistantAlarmTimerControllerMock() = default;
  AssistantAlarmTimerControllerMock(const AssistantAlarmTimerControllerMock&) =
      delete;
  AssistantAlarmTimerControllerMock& operator=(
      const AssistantAlarmTimerControllerMock&) = delete;
  ~AssistantAlarmTimerControllerMock() override = default;

  // ash::AssistantAlarmTimerController:
  MOCK_METHOD((const ash::AssistantAlarmTimerModel*),
              GetModel,
              (),
              (const, override));

  MOCK_METHOD(void,
              OnTimerStateChanged,
              (std::vector<ash::AssistantTimerPtr>),
              (override));
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

    service_context_ = std::make_unique<FakeServiceContext>();
    service_context_
        ->set_main_task_runner(task_environment.GetMainThreadTaskRunner())
        .set_power_manager_client(PowerManagerClient::Get())
        .set_assistant_state(&assistant_state_);

    CreateAssistantManagerServiceImpl(/*libassistant_config=*/{});
  }

  void CreateAssistantManagerServiceImpl(
      base::Optional<std::string> s3_server_uri_override = base::nullopt) {
    assistant_manager_service_.reset();
    delegate_ = nullptr;

    auto delegate = std::make_unique<FakeAssistantManagerServiceDelegate>();
    delegate_ = delegate.get();

    assistant_manager_service_ = std::make_unique<AssistantManagerServiceImpl>(
        service_context_.get(), std::move(delegate),
        shared_url_loader_factory_->Clone(), s3_server_uri_override);
  }

  void TearDown() override {
    assistant_manager_service_.reset();
    PowerManagerClient::Shutdown();
  }

  AssistantManagerServiceImpl* assistant_manager_service() {
    return assistant_manager_service_.get();
  }

  ash::AssistantState* assistant_state() { return &assistant_state_; }

  FakeAssistantManagerServiceDelegate* delegate() { return delegate_; }

  FakeAssistantManager* fake_assistant_manager() {
    return delegate_->assistant_manager();
  }

  FakeAssistantManagerInternal* fake_assistant_manager_internal() {
    return delegate_->assistant_manager_internal();
  }

  FakeAlarmTimerManager* fake_alarm_timer_manager() {
    return static_cast<FakeAlarmTimerManager*>(
        fake_assistant_manager_internal()->GetAlarmTimerManager());
  }

  FakeServiceContext* fake_service_context() { return service_context_.get(); }

  action::CrosActionModule* action_module() {
    return assistant_manager_service_->action_module_for_testing();
  }

  void Start() {
    assistant_manager_service()->Start(UserInfo("<user-id>", "<access-token>"),
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

  ScopedAssistantClient assistant_client_;
  ScopedDeviceActions device_actions_;
  FullyInitializedAssistantState assistant_state_;

  std::unique_ptr<FakeServiceContext> service_context_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  FakeAssistantManagerServiceDelegate* delegate_;

  std::unique_ptr<AssistantManagerServiceImpl> assistant_manager_service_;

  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceImplTest);
};

// Tests if the JSON string contains the given path with the given value
#define EXPECT_HAS_PATH_WITH_VALUE(config_string, path, expected_value)    \
  ({                                                                       \
    base::Optional<base::Value> config =                                   \
        base::JSONReader::Read(config_string);                             \
    ASSERT_TRUE(config.has_value());                                       \
    const base::Value* actual = config->FindPath(path);                    \
    base::Value expected = base::Value(expected_value);                    \
    ASSERT_NE(actual, nullptr)                                             \
        << "Path '" << path << "' not found in config: " << config_string; \
    EXPECT_EQ(*actual, expected);                                          \
  })

}  // namespace

TEST_F(AssistantManagerServiceImplTest, StateShouldStartAsStopped) {
  EXPECT_STATE(AssistantManagerService::STOPPED);
}

TEST_F(AssistantManagerServiceImplTest,
       StateShouldChangeToStartingAfterCallingStart) {
  Start();

  EXPECT_STATE(AssistantManagerService::STARTING);
}

TEST_F(AssistantManagerServiceImplTest,
       StateShouldRemainStartingUntilLibassistantIsStarted) {
  Start();

  fake_assistant_manager()->BlockStartCalls();
  RunUntilIdle();

  EXPECT_STATE(AssistantManagerService::STARTING);

  fake_assistant_manager()->UnblockStartCalls();
  WaitUntilStartIsFinished();

  EXPECT_STATE(AssistantManagerService::STARTED);
}

TEST_F(AssistantManagerServiceImplTest,
       StateShouldBecomeRunningAfterLibassistantSignalsOnStartFinished) {
  NiceMock<AssistantAlarmTimerControllerMock> alarm_timer_controller;
  fake_service_context()->set_assistant_alarm_timer_controller(
      &alarm_timer_controller);

  Start();
  WaitUntilStartIsFinished();

  fake_assistant_manager()->device_state_listener()->OnStartFinished();

  EXPECT_STATE(AssistantManagerService::RUNNING);
}

TEST_F(AssistantManagerServiceImplTest, ShouldSetStateToStoppedAfterStopping) {
  Start();
  WaitUntilStartIsFinished();
  EXPECT_STATE(AssistantManagerService::STARTED);

  assistant_manager_service()->Stop();
  EXPECT_STATE(AssistantManagerService::STOPPED);
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
       ShouldPassUserInfoToAssistantManagerWhenStarting) {
  assistant_manager_service()->Start(UserInfo("<user-id>", "<access-token>"),
                                     /*enable_hotword=*/false);
  WaitUntilStartIsFinished();

  EXPECT_EQ("<user-id>", fake_assistant_manager()->user_id());
  EXPECT_EQ("<access-token>", fake_assistant_manager()->access_token());
}

TEST_F(AssistantManagerServiceImplTest, ShouldPassUserInfoToAssistantManager) {
  Start();
  WaitUntilStartIsFinished();

  assistant_manager_service()->SetUser(
      UserInfo("<new-user-id>", "<new-access-token>"));

  EXPECT_EQ("<new-user-id>", fake_assistant_manager()->user_id());
  EXPECT_EQ("<new-access-token>", fake_assistant_manager()->access_token());
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldPassEmptyUserInfoToAssistantManager) {
  Start();
  WaitUntilStartIsFinished();

  assistant_manager_service()->SetUser(base::nullopt);

  EXPECT_EQ(kNoValue, fake_assistant_manager()->user_id());
  EXPECT_EQ(kNoValue, fake_assistant_manager()->access_token());
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldNotCrashWhenSettingUserInfoBeforeStartIsFinished) {
  EXPECT_STATE(AssistantManagerService::STOPPED);
  assistant_manager_service()->SetUser(UserInfo("<user-id>", "<access-token>"));

  Start();
  EXPECT_STATE(AssistantManagerService::STARTING);
  assistant_manager_service()->SetUser(UserInfo("<user-id>", "<access-token>"));
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldPassS3ServerUriOverrideToAssistantManager) {
  CreateAssistantManagerServiceImpl("the-uri-override");

  Start();
  WaitUntilStartIsFinished();

  EXPECT_HAS_PATH_WITH_VALUE(delegate()->libassistant_config(),
                             "testing.s3_grpc_server_uri", "the-uri-override");
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
  NiceMock<AssistantAlarmTimerControllerMock> alarm_timer_controller;
  fake_service_context()->set_assistant_alarm_timer_controller(
      &alarm_timer_controller);

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

TEST_F(AssistantManagerServiceImplTest,
       ShouldUpdateActionModuleWhenAmbientModeStateChanged) {
  EXPECT_FALSE(action_module()->IsAmbientModeEnabledForTesting());

  assistant_manager_service()->EnableAmbientMode(true);
  EXPECT_TRUE(action_module()->IsAmbientModeEnabledForTesting());

  assistant_manager_service()->EnableAmbientMode(false);
  EXPECT_FALSE(action_module()->IsAmbientModeEnabledForTesting());
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldNotifyAlarmTimerControllerOfOnlyRingingTimersInV1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kAssistantTimersV2);

  Start();
  WaitUntilStartIsFinished();
  assistant_manager_service()->OnStartFinished();

  StrictMock<AssistantAlarmTimerControllerMock> alarm_timer_controller;
  fake_service_context()->set_assistant_alarm_timer_controller(
      &alarm_timer_controller);

  EXPECT_CALL(alarm_timer_controller, OnTimerStateChanged)
      .WillOnce(Invoke([](auto timers) {
        ASSERT_EQ(1u, timers.size());
        EXPECT_EQ(ash::AssistantTimerState::kFired, timers[0]->state);
      }));

  std::vector<assistant_client::AlarmTimerEvent> events;

  // Ignore NONE, ALARMs, and SCHEDULED/PAUSED timers.
  AddAlarmTimerEvent(&events, assistant_client::AlarmTimerEvent::Type::NONE);
  AddAlarmTimerEvent(&events, assistant_client::AlarmTimerEvent::Type::ALARM);
  AddTimerEvent(&events, assistant_client::Timer::State::SCHEDULED);
  AddTimerEvent(&events, assistant_client::Timer::State::PAUSED);

  // Accept FIRED timers.
  AddTimerEvent(&events, assistant_client::Timer::State::FIRED);

  fake_alarm_timer_manager()->SetAllEvents(std::move(events));
  fake_alarm_timer_manager()->NotifyRingingStateListeners();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldNotifyAlarmTimerControllerOfAnyTimersInV2) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAssistantTimersV2);

  StrictMock<AssistantAlarmTimerControllerMock> alarm_timer_controller;
  fake_service_context()->set_assistant_alarm_timer_controller(
      &alarm_timer_controller);

  // We expect OnTimerStateChanged() to be invoked when starting LibAssistant.
  EXPECT_CALL(alarm_timer_controller, OnTimerStateChanged).Times(1);

  Start();
  WaitUntilStartIsFinished();
  assistant_manager_service()->OnStartFinished();

  testing::Mock::VerifyAndClearExpectations(&alarm_timer_controller);

  EXPECT_CALL(alarm_timer_controller, OnTimerStateChanged)
      .WillOnce(Invoke([](auto timers) {
        ASSERT_EQ(3u, timers.size());
        EXPECT_EQ(ash::AssistantTimerState::kScheduled, timers[0]->state);
        EXPECT_EQ(ash::AssistantTimerState::kPaused, timers[1]->state);
        EXPECT_EQ(ash::AssistantTimerState::kFired, timers[2]->state);
      }));

  std::vector<assistant_client::AlarmTimerEvent> events;

  // Ignore NONE and ALARMs.
  AddAlarmTimerEvent(&events, assistant_client::AlarmTimerEvent::Type::NONE);
  AddAlarmTimerEvent(&events, assistant_client::AlarmTimerEvent::Type::ALARM);

  // Accept SCHEDULED/PAUSED/FIRED timers.
  AddTimerEvent(&events, assistant_client::Timer::State::SCHEDULED);
  AddTimerEvent(&events, assistant_client::Timer::State::PAUSED);
  AddTimerEvent(&events, assistant_client::Timer::State::FIRED);

  fake_alarm_timer_manager()->SetAllEvents(std::move(events));
  fake_alarm_timer_manager()->NotifyRingingStateListeners();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldNotifyAlarmTimerControllerOfTimersWhenStartingLibAssistantInV2) {
  // Enable timers V2.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAssistantTimersV2);

  // Pre-populate the AlarmTimerManager with a single scheduled timer.
  std::vector<assistant_client::AlarmTimerEvent> events;
  AddTimerEvent(&events, assistant_client::Timer::State::SCHEDULED);
  fake_alarm_timer_manager()->SetAllEvents(std::move(events));

  // Bind AssistantAlarmTimerController.
  StrictMock<AssistantAlarmTimerControllerMock> alarm_timer_controller;
  fake_service_context()->set_assistant_alarm_timer_controller(
      &alarm_timer_controller);

  // Expect |timers| to be sent to AssistantAlarmTimerController.  Verify
  // AssistantAlarmTimerController is notified of the scheduled timer.
  EXPECT_CALL(alarm_timer_controller, OnTimerStateChanged)
      .WillOnce(Invoke([](auto timers) {
        ASSERT_EQ(1u, timers.size());
        EXPECT_EQ(ash::AssistantTimerState::kScheduled, timers[0]->state);
      }));

  // Start LibAssistant.
  Start();
  WaitUntilStartIsFinished();
  assistant_manager_service()->OnStartFinished();
}

}  // namespace assistant
}  // namespace chromeos
