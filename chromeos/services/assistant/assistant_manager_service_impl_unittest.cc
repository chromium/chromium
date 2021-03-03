// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_manager_service_impl.h"

#include <string>
#include <utility>

#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/test_support/fake_alarm_timer_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/assistant/test_support/expect_utils.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/services/assistant/assistant_manager_service.h"
#include "chromeos/services/assistant/proxy/libassistant_service_host.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/cpp/migration/fake_assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/assistant/service_context.h"
#include "chromeos/services/assistant/test_support/fake_libassistant_service.h"
#include "chromeos/services/assistant/test_support/fake_service_context.h"
#include "chromeos/services/assistant/test_support/fully_initialized_assistant_state.h"
#include "chromeos/services/assistant/test_support/libassistant_media_controller_mock.h"
#include "chromeos/services/assistant/test_support/mock_assistant_interaction_subscriber.h"
#include "chromeos/services/assistant/test_support/scoped_assistant_client.h"
#include "chromeos/services/assistant/test_support/scoped_device_actions.h"
#include "chromeos/services/libassistant/public/cpp/assistant_timer.h"
#include "chromeos/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_session.mojom-shared.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

using media_session::mojom::MediaSessionAction;
using testing::_;
using testing::ElementsAre;
using testing::Invoke;
using testing::NiceMock;
using testing::StrictMock;
using CommunicationErrorType = AssistantManagerService::CommunicationErrorType;
using UserInfo = AssistantManagerService::UserInfo;
using libassistant::mojom::ServiceState;
using libassistant::mojom::SpeakerIdEnrollmentStatus;

namespace {

const char* kNoValue = FakeAssistantManager::kNoValue;

#define EXPECT_STATE(_state) \
  EXPECT_EQ(_state, assistant_manager_service()->GetState());

// Adds an AlarmTimerEvent of the given |type| to |events|.
void AddAlarmTimerEvent(std::vector<assistant_client::AlarmTimerEvent>* events,
                        assistant_client::AlarmTimerEvent::Type type) {
  events->push_back(assistant_client::AlarmTimerEvent());
  events->back().type = type;
}

// Adds an AlarmTimerEvent of type TIMER with the given |state| to |events|.
void AddTimerEvent(std::vector<assistant_client::AlarmTimerEvent>* events,
                   assistant_client::Timer::State state) {
  AddAlarmTimerEvent(events, assistant_client::AlarmTimerEvent::TIMER);
  events->back().timer_data.state = state;
}

// Return the list of all libassistant error codes that are considered to be
// authentication errors. This list is created on demand as there is no clear
// enum that defines these, and we don't want to hard code this list in the
// test.
std::vector<int> GetAuthenticationErrorCodes() {
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
std::vector<int> GetNonAuthenticationErrorCodes() {
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
              (const std::vector<AssistantTimer>&),
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

class FakeLibassistantServiceHost : public LibassistantServiceHost {
 public:
  explicit FakeLibassistantServiceHost(FakeLibassistantService* service)
      : service_(service) {}

  void Launch(
      mojo::PendingReceiver<LibassistantServiceMojom> receiver) override {
    service_->Bind(std::move(receiver));
  }
  void Stop() override { service_->Unbind(); }

  void SetInitializeCallback(
      base::OnceCallback<void(assistant_client::AssistantManager*,
                              assistant_client::AssistantManagerInternal*)>
          callback) override {
    service_->service_controller().SetInitializeCallback(std::move(callback));
  }

 private:
  FakeLibassistantService* service_;
};

class StateObserverMock : public AssistantManagerService::StateObserver {
 public:
  StateObserverMock() = default;
  ~StateObserverMock() override = default;

  MOCK_METHOD(void, OnStateChanged, (AssistantManagerService::State new_state));

 private:
  DISALLOW_COPY_AND_ASSIGN(StateObserverMock);
};

class FakeLibassistantV1Api : public LibassistantV1Api {
 public:
  explicit FakeLibassistantV1Api(FakeAssistantManager* assistant_manager)
      : LibassistantV1Api(assistant_manager,
                          &assistant_manager->assistant_manager_internal()) {
    SetActionModule(&action_module_);
  }

 private:
  action::CrosActionModule action_module_{features::IsAppSupportEnabled(),
                                          features::IsWaitSchedulingEnabled()};
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

    alarm_timer_controller_ =
        std::make_unique<NiceMock<AssistantAlarmTimerControllerMock>>();

    service_context_ = std::make_unique<FakeServiceContext>();
    service_context_
        ->set_main_task_runner(task_environment().GetMainThreadTaskRunner())
        .set_power_manager_client(PowerManagerClient::Get())
        .set_assistant_state(&assistant_state_)
        .set_assistant_alarm_timer_controller(alarm_timer_controller_.get());

    CreateAssistantManagerServiceImpl();
  }

  void CreateAssistantManagerServiceImpl(
      base::Optional<std::string> s3_server_uri_override = base::nullopt,
      base::Optional<std::string> device_id_override = base::nullopt) {
    // We can not have 2 instances of |AssistantManagerServiceImpl| at the same
    // time, so we must destroy the old one before creating a new one.
    assistant_manager_service_.reset();

    assistant_manager_service_ = std::make_unique<AssistantManagerServiceImpl>(
        service_context_.get(),
        std::make_unique<FakeAssistantManagerServiceDelegate>(),
        shared_url_loader_factory_->Clone(), s3_server_uri_override,
        device_id_override,
        std::make_unique<FakeLibassistantServiceHost>(&libassistant_service_));
  }

  void TearDown() override {
    assistant_manager_service_.reset();
    PowerManagerClient::Shutdown();
  }

  FakeServiceController& mojom_service_controller() {
    return libassistant_service_.service_controller();
  }

  FakeLibassistantService& mojom_libassistant_service() {
    return libassistant_service_;
  }

  AssistantManagerServiceImpl* assistant_manager_service() {
    return assistant_manager_service_.get();
  }

  AssistantSettings& assistant_settings() {
    auto* result = assistant_manager_service()->GetAssistantSettings();
    DCHECK(result);
    return *result;
  }

  FullyInitializedAssistantState& assistant_state() { return assistant_state_; }

  FakeAssistantManager* fake_assistant_manager() {
    return assistant_manager_.get();
  }

  FakeAssistantManagerInternal* fake_assistant_manager_internal() {
    return &fake_assistant_manager()->assistant_manager_internal();
  }

  FakeAlarmTimerManager* fake_alarm_timer_manager() {
    return static_cast<FakeAlarmTimerManager*>(
        fake_assistant_manager_internal()->GetAlarmTimerManager());
  }

  FakeServiceContext* fake_service_context() { return service_context_.get(); }

  action::CrosActionModule* action_module() {
    return assistant_manager_service_->action_module();
  }

  // Replace the |AssistantAlarmTimerControllerMock| with a |StrictMock|.
  void UseStrictAlarmTimerControllerMock() {
    // We can not have 2 instances of |AssistantAlarmTimerController| at the
    // same time, so we must destroy the current version first.
    alarm_timer_controller_ = nullptr;
    alarm_timer_controller_ =
        std::make_unique<StrictMock<AssistantAlarmTimerControllerMock>>();
    fake_service_context()->set_assistant_alarm_timer_controller(
        alarm_timer_controller_.get());
  }

  AssistantAlarmTimerControllerMock& alarm_timer_controller_mock() {
    return *alarm_timer_controller_;
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void Start() {
    assistant_manager_service()->Start(UserInfo("<user-id>", "<access-token>"),
                                       /*enable_hotword=*/false);
  }

  // Start Libassistant, and wait until it is running.
  void StartAndWaitForRunning() {
    Start();
    WaitForState(AssistantManagerService::STARTED);
    mojom_service_controller().SetState(ServiceState::kRunning);
    WaitForState(AssistantManagerService::RUNNING);
  }

  void RunUntilIdle() {
    // First ensure our mojom thread is finished.
    background_thread().FlushForTesting();
    // Then handle any callbacks.
    base::RunLoop().RunUntilIdle();
  }

  // Adds a state observer mock, and add the expectation for the fact that it
  // auto-fires the observer.
  void AddStateObserver(StateObserverMock* observer) {
    EXPECT_CALL(*observer,
                OnStateChanged(assistant_manager_service()->GetState()));
    assistant_manager_service()->AddAndFireStateObserver(observer);
  }

  void WaitForState(AssistantManagerService::State expected_state) {
    test::ExpectResult(
        expected_state,
        base::BindRepeating(&AssistantManagerServiceImpl::GetState,
                            base::Unretained(assistant_manager_service())),
        "AssistantManagerStateImpl");
  }

  // Raise all the |libassistant_error_codes| as communication errors from
  // libassistant, and check that they are reported to our
  // |AssistantCommunicationErrorObserver| as errors of type |expected_type|.
  void TestCommunicationErrors(const std::vector<int>& libassistant_error_codes,
                               CommunicationErrorType expected_error) {
    Start();
    WaitForState(AssistantManagerService::STARTED);

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

  void SetAssistantManagerInternal(std::unique_ptr<FakeAssistantManagerInternal>
                                       assistant_manager_internal) {
    assistant_manager_->set_assistant_manager_internal(
        std::move(assistant_manager_internal));
    libassistant_v1_api_.reset();
    libassistant_v1_api_ =
        std::make_unique<FakeLibassistantV1Api>(assistant_manager_.get());
  }

  void SetAssistantManager(
      std::unique_ptr<FakeAssistantManager> assistant_manager) {
    assistant_manager_ = std::move(assistant_manager);
    libassistant_v1_api_.reset();
    libassistant_v1_api_ =
        std::make_unique<FakeLibassistantV1Api>(assistant_manager_.get());
  }

 private:
  base::Thread& background_thread() {
    return assistant_manager_service()->GetBackgroundThreadForTesting();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  ScopedAssistantClient assistant_client_;
  ScopedDeviceActions device_actions_;
  FullyInitializedAssistantState assistant_state_;

  // Fake implementation of the Libassistant Mojom service.
  FakeLibassistantService libassistant_service_;

  std::unique_ptr<FakeAssistantManager> assistant_manager_{
      std::make_unique<FakeAssistantManager>()};
  std::unique_ptr<FakeLibassistantV1Api> libassistant_v1_api_{
      std::make_unique<FakeLibassistantV1Api>(assistant_manager_.get())};

  std::unique_ptr<AssistantAlarmTimerControllerMock> alarm_timer_controller_;
  std::unique_ptr<FakeServiceContext> service_context_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<AssistantManagerServiceImpl> assistant_manager_service_;

  DISALLOW_COPY_AND_ASSIGN(AssistantManagerServiceImplTest);
};

class SpeakerIdEnrollmentControllerMock
    : public ::chromeos::libassistant::mojom::SpeakerIdEnrollmentController {
 public:
  SpeakerIdEnrollmentControllerMock() = default;
  SpeakerIdEnrollmentControllerMock(const SpeakerIdEnrollmentControllerMock&) =
      delete;
  SpeakerIdEnrollmentControllerMock& operator=(
      const SpeakerIdEnrollmentControllerMock&) = delete;
  ~SpeakerIdEnrollmentControllerMock() override = default;

  // ::chromeos::libassistant::mojom::SpeakerIdEnrollmentController
  // implementation:
  MOCK_METHOD(
      void,
      StartSpeakerIdEnrollment,
      (const std::string& user_gaia_id,
       bool skip_cloud_enrollment,
       ::mojo::PendingRemote<libassistant::mojom::SpeakerIdEnrollmentClient>
           client));
  MOCK_METHOD(void, StopSpeakerIdEnrollment, ());
  MOCK_METHOD(void,
              GetSpeakerIdEnrollmentStatus,
              (const std::string& user_gaia_id,
               GetSpeakerIdEnrollmentStatusCallback callback));

  void Bind(
      mojo::PendingReceiver<libassistant::mojom::SpeakerIdEnrollmentController>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  void Bind(FakeLibassistantService& service) {
    Bind(service.GetSpeakerIdEnrollmentControllerPendingReceiver());
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<SpeakerIdEnrollmentController> receiver_{this};
};

class SpeakerIdEnrollmentClientMock : public SpeakerIdEnrollmentClient {
 public:
  SpeakerIdEnrollmentClientMock() = default;
  SpeakerIdEnrollmentClientMock(const SpeakerIdEnrollmentClientMock&) = delete;
  SpeakerIdEnrollmentClientMock& operator=(
      const SpeakerIdEnrollmentClientMock&) = delete;
  ~SpeakerIdEnrollmentClientMock() override = default;

  base::WeakPtr<SpeakerIdEnrollmentClientMock> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // SpeakerIdEnrollmentClient implementation:
  MOCK_METHOD(void, OnListeningHotword, ());
  MOCK_METHOD(void, OnProcessingHotword, ());
  MOCK_METHOD(void, OnSpeakerIdEnrollmentDone, ());
  MOCK_METHOD(void, OnSpeakerIdEnrollmentFailure, ());

 private:
  base::WeakPtrFactory<SpeakerIdEnrollmentClientMock> weak_factory_{this};
};

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
       StateShouldRemainStartingUntilLibassistantServiceIsStarted) {
  mojom_service_controller().BlockStartCalls();

  Start();
  WaitForState(AssistantManagerService::STARTING);

  mojom_service_controller().UnblockStartCalls();
  WaitForState(AssistantManagerService::STARTED);
}

TEST_F(AssistantManagerServiceImplTest,
       StateShouldBecomeRunningAfterLibassistantSignalsRunningState) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  mojom_service_controller().SetState(ServiceState::kRunning);

  WaitForState(AssistantManagerService::RUNNING);
}

TEST_F(AssistantManagerServiceImplTest, ShouldSetStateToStoppedAfterStopping) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  assistant_manager_service()->Stop();
  WaitForState(AssistantManagerService::STOPPED);
}

TEST_F(AssistantManagerServiceImplTest, ShouldAllowRestartingAfterStopping) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  assistant_manager_service()->Stop();
  WaitForState(AssistantManagerService::STOPPED);

  Start();
  WaitForState(AssistantManagerService::STARTED);
}

TEST_F(AssistantManagerServiceImplTest, ShouldNotResetDataWhenStopping) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  assistant_manager_service()->Stop();
  WaitForState(AssistantManagerService::STOPPED);
  RunUntilIdle();

  EXPECT_EQ(false, mojom_service_controller().has_data_been_reset());
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldResetDataWhenAssistantIsDisabled) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  assistant_state().SetAssistantEnabled(false);
  assistant_manager_service()->Stop();
  WaitForState(AssistantManagerService::STOPPED);
  RunUntilIdle();

  EXPECT_EQ(true, mojom_service_controller().has_data_been_reset());
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

  WaitForState(AssistantManagerService::STARTED);

  EXPECT_EQ("<user-id>", mojom_service_controller().gaia_id());
  EXPECT_EQ("<access-token>", mojom_service_controller().access_token());
}

TEST_F(AssistantManagerServiceImplTest, ShouldPassUserInfoToAssistantManager) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  assistant_manager_service()->SetUser(
      UserInfo("<new-user-id>", "<new-access-token>"));
  RunUntilIdle();

  EXPECT_EQ("<new-user-id>", mojom_service_controller().gaia_id());
  EXPECT_EQ("<new-access-token>", mojom_service_controller().access_token());
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldPassEmptyUserInfoToAssistantManager) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  assistant_manager_service()->SetUser(base::nullopt);
  RunUntilIdle();

  EXPECT_EQ(kNoValue, mojom_service_controller().gaia_id());
  EXPECT_EQ(kNoValue, mojom_service_controller().access_token());
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
       ShouldPassS3ServerUriOverrideToMojomService) {
  CreateAssistantManagerServiceImpl("the-uri-override");

  Start();
  WaitForState(AssistantManagerService::STARTED);

  EXPECT_EQ(mojom_service_controller()
                .libassistant_config()
                .s3_server_uri_override.value_or("<none>"),
            "the-uri-override");
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldPassDeviceIdOverrideToMojomService) {
  CreateAssistantManagerServiceImpl(
      /*s3_server_uri_override=*/base::nullopt, "the-device-id-override");

  Start();
  WaitForState(AssistantManagerService::STARTED);

  EXPECT_EQ(mojom_service_controller()
                .libassistant_config()
                .device_id_override.value_or("<none>"),
            "the-device-id-override");
}

TEST_F(AssistantManagerServiceImplTest, ShouldPauseMediaManagerOnPause) {
  StrictMock<LibassistantMediaControllerMock> mock;

  StartAndWaitForRunning();

  mock.Bind(mojom_libassistant_service().GetMediaControllerPendingReceiver());

  EXPECT_CALL(mock, PauseInternalMediaPlayer);

  assistant_manager_service()->UpdateInternalMediaPlayerStatus(
      MediaSessionAction::kPause);
  mock.FlushForTesting();
}

TEST_F(AssistantManagerServiceImplTest, ShouldResumeMediaManagerOnPlay) {
  StrictMock<LibassistantMediaControllerMock> mock;

  StartAndWaitForRunning();

  mock.Bind(mojom_libassistant_service().GetMediaControllerPendingReceiver());

  EXPECT_CALL(mock, ResumeInternalMediaPlayer);

  assistant_manager_service()->UpdateInternalMediaPlayerStatus(
      MediaSessionAction::kPlay);
  mock.FlushForTesting();
}

TEST_F(AssistantManagerServiceImplTest, ShouldIgnoreOtherMediaManagerActions) {
  StrictMock<LibassistantMediaControllerMock> mock;

  const auto unsupported_media_session_actions = {
      MediaSessionAction::kPreviousTrack, MediaSessionAction::kNextTrack,
      MediaSessionAction::kSeekBackward,  MediaSessionAction::kSeekForward,
      MediaSessionAction::kSkipAd,        MediaSessionAction::kStop,
      MediaSessionAction::kSeekTo,        MediaSessionAction::kScrubTo,
  };

  StartAndWaitForRunning();

  mock.Bind(mojom_libassistant_service().GetMediaControllerPendingReceiver());

  for (auto action : unsupported_media_session_actions) {
    // If this is not ignored, |mock| will complain about an uninterested call.
    assistant_manager_service()->UpdateInternalMediaPlayerStatus(action);
  }

  mock.FlushForTesting();
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldNotCrashWhenMediaManagerIsAbsent) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

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
  WaitForState(AssistantManagerService::STARTED);

  assistant_manager_service()->RemoveStateObserver(&observer);
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldFireStateObserverWhenLibAssistantServiceIsRunning) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  StrictMock<StateObserverMock> observer;
  AddStateObserver(&observer);
  EXPECT_CALL(observer,
              OnStateChanged(AssistantManagerService::State::RUNNING));

  mojom_service_controller().SetState(ServiceState::kRunning);
  WaitForState(AssistantManagerService::RUNNING);

  assistant_manager_service()->RemoveStateObserver(&observer);
}

TEST_F(AssistantManagerServiceImplTest, ShouldFireStateObserverWhenStopping) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

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
       ShouldNotifyAlarmTimerControllerOfOnlyRingingTimersInV1) {
  UseStrictAlarmTimerControllerMock();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kAssistantTimersV2);

  StartAndWaitForRunning();

  EXPECT_CALL(alarm_timer_controller_mock(), OnTimerStateChanged)
      .WillOnce(Invoke([](const auto& timers) {
        ASSERT_EQ(1u, timers.size());
        EXPECT_EQ(AssistantTimerState::kFired, timers[0].state);
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
  UseStrictAlarmTimerControllerMock();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAssistantTimersV2);

  // We expect OnTimerStateChanged() to be invoked when starting LibAssistant.
  EXPECT_CALL(alarm_timer_controller_mock(), OnTimerStateChanged).Times(1);

  StartAndWaitForRunning();

  testing::Mock::VerifyAndClearExpectations(&alarm_timer_controller_mock());

  EXPECT_CALL(alarm_timer_controller_mock(), OnTimerStateChanged)
      .WillOnce(Invoke([](const auto& timers) {
        ASSERT_EQ(3u, timers.size());
        EXPECT_EQ(AssistantTimerState::kScheduled, timers[0].state);
        EXPECT_EQ(AssistantTimerState::kPaused, timers[1].state);
        EXPECT_EQ(AssistantTimerState::kFired, timers[2].state);
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
  UseStrictAlarmTimerControllerMock();
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAssistantTimersV2);

  // Pre-populate the AlarmTimerManager with a single scheduled timer.
  std::vector<assistant_client::AlarmTimerEvent> events;
  AddTimerEvent(&events, assistant_client::Timer::State::SCHEDULED);
  fake_alarm_timer_manager()->SetAllEvents(std::move(events));

  // Expect |timers| to be sent to AssistantAlarmTimerController.  Verify
  // AssistantAlarmTimerController is notified of the scheduled timer.
  EXPECT_CALL(alarm_timer_controller_mock(), OnTimerStateChanged)
      .WillOnce(Invoke([](const auto& timers) {
        ASSERT_EQ(1u, timers.size());
        EXPECT_EQ(AssistantTimerState::kScheduled, timers[0].state);
      }));

  // Start LibAssistant.
  StartAndWaitForRunning();
}

class AssistantManagerMock : public FakeAssistantManager {
 public:
  AssistantManagerMock() = default;
  ~AssistantManagerMock() override = default;

  MOCK_METHOD(void, StartAssistantInteraction, (), (override));
};

class AssistantManagerInternalMock : public FakeAssistantManagerInternal {
 public:
  AssistantManagerInternalMock() = default;
  ~AssistantManagerInternalMock() override = default;

  MOCK_METHOD(void, StopAssistantInteractionInternal, (bool), (override));
};

TEST_F(AssistantManagerServiceImplTest, ShouldStopInteractionAfterDelay) {
  // Start LibAssistant.
  Start();
  WaitForState(AssistantManagerService::STARTED);

  auto assistant_manager_internal_mock =
      std::make_unique<AssistantManagerInternalMock>();
  auto* mock_ptr = assistant_manager_internal_mock.get();
  SetAssistantManagerInternal(std::move(assistant_manager_internal_mock));

  EXPECT_CALL(*mock_ptr, StopAssistantInteractionInternal).Times(0);

  assistant_manager_service()->StopActiveInteraction(true);
  testing::Mock::VerifyAndClearExpectations(mock_ptr);

  WAIT_FOR_CALL(*mock_ptr, StopAssistantInteractionInternal);
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldStopInteractionImmediatelyBeforeNewInteraction) {
  // Start LibAssistant.
  Start();
  WaitForState(AssistantManagerService::STARTED);

  auto assistant_manager_mock = std::make_unique<AssistantManagerMock>();
  auto assistant_manager_internal_mock =
      std::make_unique<AssistantManagerInternalMock>();
  auto* assistant_manager_mock_ptr = assistant_manager_mock.get();
  auto* assistant_manager_internal_mock_ptr =
      assistant_manager_internal_mock.get();

  assistant_manager_mock->set_assistant_manager_internal(
      std::move(assistant_manager_internal_mock));
  SetAssistantManager(std::move(assistant_manager_mock));

  EXPECT_CALL(*assistant_manager_internal_mock_ptr,
              StopAssistantInteractionInternal)
      .Times(0);

  assistant_manager_service()->StopActiveInteraction(true);
  testing::Mock::VerifyAndClearExpectations(
      assistant_manager_internal_mock_ptr);

  EXPECT_CALL(*assistant_manager_internal_mock_ptr,
              StopAssistantInteractionInternal)
      .Times(1);
  EXPECT_CALL(*assistant_manager_mock_ptr, StartAssistantInteraction).Times(1);
  assistant_manager_service()->StartVoiceInteraction();
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldStartSpeakerIdEnrollmentWhenRequested) {
  NiceMock<SpeakerIdEnrollmentClientMock> client_mock;
  Start();
  WaitForState(AssistantManagerService::STARTED);

  StrictMock<SpeakerIdEnrollmentControllerMock> mojom_mock;
  mojom_mock.Bind(mojom_libassistant_service());

  EXPECT_CALL(mojom_mock, StartSpeakerIdEnrollment);

  assistant_settings().StartSpeakerIdEnrollment(/*skip_cloud_enrollment=*/false,
                                                client_mock.GetWeakPtr());

  mojom_mock.FlushForTesting();
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldSendGaiaIdDuringSpeakerIdEnrollment) {
  NiceMock<SpeakerIdEnrollmentClientMock> client_mock;
  fake_service_context()->set_primary_account_gaia_id("gaia user id");
  Start();
  WaitForState(AssistantManagerService::STARTED);

  StrictMock<SpeakerIdEnrollmentControllerMock> mojom_mock;
  mojom_mock.Bind(mojom_libassistant_service());

  EXPECT_CALL(mojom_mock, StartSpeakerIdEnrollment("gaia user id", _, _));

  assistant_settings().StartSpeakerIdEnrollment(/*skip_cloud_enrollment=*/false,
                                                client_mock.GetWeakPtr());

  mojom_mock.FlushForTesting();
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldSendSkipCloudEnrollmentDuringSpeakerIdEnrollment) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  StrictMock<SpeakerIdEnrollmentControllerMock> mojom_mock;
  mojom_mock.Bind(mojom_libassistant_service());

  {
    NiceMock<SpeakerIdEnrollmentClientMock> client_mock;

    EXPECT_CALL(mojom_mock, StartSpeakerIdEnrollment(_, true, _));

    assistant_settings().StartSpeakerIdEnrollment(
        /*skip_cloud_enrollment=*/true, client_mock.GetWeakPtr());
    mojom_mock.FlushForTesting();
  }

  {
    NiceMock<SpeakerIdEnrollmentClientMock> client_mock;

    EXPECT_CALL(mojom_mock, StartSpeakerIdEnrollment(_, false, _));

    assistant_settings().StartSpeakerIdEnrollment(
        /*skip_cloud_enrollment=*/false, client_mock.GetWeakPtr());
    mojom_mock.FlushForTesting();
  }
}

TEST_F(AssistantManagerServiceImplTest, ShouldSendStopSpeakerIdEnrollment) {
  NiceMock<SpeakerIdEnrollmentClientMock> client_mock;
  Start();
  WaitForState(AssistantManagerService::STARTED);

  StrictMock<SpeakerIdEnrollmentControllerMock> mojom_mock;
  mojom_mock.Bind(mojom_libassistant_service());

  EXPECT_CALL(mojom_mock, StopSpeakerIdEnrollment);

  assistant_settings().StopSpeakerIdEnrollment();
  mojom_mock.FlushForTesting();
}

TEST_F(AssistantManagerServiceImplTest, ShouldSyncSpeakerIdEnrollmentStatus) {
  StrictMock<SpeakerIdEnrollmentClientMock> client_mock;
  Start();
  WaitForState(AssistantManagerService::STARTED);

  StrictMock<SpeakerIdEnrollmentControllerMock> mojom_mock;
  mojom_mock.Bind(mojom_libassistant_service());

  EXPECT_CALL(mojom_mock, GetSpeakerIdEnrollmentStatus)
      .WillOnce([](const std::string& user_gaia_id,
                   SpeakerIdEnrollmentControllerMock::
                       GetSpeakerIdEnrollmentStatusCallback callback) {
        std::move(callback).Run(
            SpeakerIdEnrollmentStatus::New(/*user_model_exists=*/true));
      });

  assistant_settings().SyncSpeakerIdEnrollmentStatus();
  mojom_mock.FlushForTesting();
}

TEST_F(AssistantManagerServiceImplTest,
       ShouldSyncSpeakerIdEnrollmentStatusWhenRunning) {
  AssistantManagerServiceImpl::ResetIsFirstInitFlagForTesting();

  StrictMock<SpeakerIdEnrollmentClientMock> client_mock;
  StrictMock<SpeakerIdEnrollmentControllerMock> mojom_mock;
  RunUntilIdle();

  mojom_mock.Bind(mojom_libassistant_service());

  EXPECT_CALL(mojom_mock, GetSpeakerIdEnrollmentStatus)
      .WillOnce([](const std::string& user_gaia_id,
                   SpeakerIdEnrollmentControllerMock::
                       GetSpeakerIdEnrollmentStatusCallback callback) {
        std::move(callback).Run(
            SpeakerIdEnrollmentStatus::New(/*user_model_exists=*/true));
      });

  StartAndWaitForRunning();

  mojom_mock.FlushForTesting();
}

}  // namespace assistant
}  // namespace chromeos
