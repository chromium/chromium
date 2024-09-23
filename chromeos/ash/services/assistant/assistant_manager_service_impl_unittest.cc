// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/assistant_manager_service_impl.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/assistant/test_support/expect_utils.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/services/assistant/assistant_manager_service.h"
#include "chromeos/ash/services/assistant/libassistant_service_host.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/service_context.h"
#include "chromeos/ash/services/assistant/test_support/fake_libassistant_service.h"
#include "chromeos/ash/services/assistant/test_support/fake_service_context.h"
#include "chromeos/ash/services/assistant/test_support/fully_initialized_assistant_state.h"
#include "chromeos/ash/services/assistant/test_support/libassistant_media_controller_mock.h"
#include "chromeos/ash/services/assistant/test_support/mock_assistant_interaction_subscriber.h"
#include "chromeos/ash/services/assistant/test_support/scoped_assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/test_support/scoped_device_actions.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"
#include "chromeos/ash/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_session.mojom-shared.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::assistant {

using libassistant::mojom::ServiceState;
using libassistant::mojom::SpeakerIdEnrollmentStatus;
using media_session::mojom::MediaSessionAction;
using testing::_;
using testing::ElementsAre;
using testing::Invoke;
using testing::NiceMock;
using testing::StrictMock;
using UserInfo = AssistantManagerService::UserInfo;

namespace {

const char* kNoValue = FakeServiceController::kNoValue;
#define EXPECT_STATE(_state) \
  EXPECT_EQ(_state, assistant_manager_service()->GetState());

class AssistantAlarmTimerControllerMock : public AssistantAlarmTimerController {
 public:
  AssistantAlarmTimerControllerMock() = default;
  AssistantAlarmTimerControllerMock(const AssistantAlarmTimerControllerMock&) =
      delete;
  AssistantAlarmTimerControllerMock& operator=(
      const AssistantAlarmTimerControllerMock&) = delete;
  ~AssistantAlarmTimerControllerMock() override = default;

  // AssistantAlarmTimerController:
  MOCK_METHOD((const AssistantAlarmTimerModel*),
              GetModel,
              (),
              (const, override));

  MOCK_METHOD(void,
              OnTimerStateChanged,
              (const std::vector<AssistantTimer>&),
              (override));
};

class FakeLibassistantServiceHost : public LibassistantServiceHost {
 public:
  explicit FakeLibassistantServiceHost(FakeLibassistantService* service)
      : service_(service) {}

  void Launch(mojo::PendingReceiver<libassistant::mojom::LibassistantService>
                  receiver) override {
    service_->Bind(std::move(receiver));
  }
  void Stop() override { service_->Unbind(); }

 private:
  raw_ptr<FakeLibassistantService> service_;
};

class StateObserverMock : public AssistantManagerService::StateObserver {
 public:
  StateObserverMock() = default;

  StateObserverMock(const StateObserverMock&) = delete;
  StateObserverMock& operator=(const StateObserverMock&) = delete;

  ~StateObserverMock() override = default;

  MOCK_METHOD(void, OnStateChanged, (AssistantManagerService::State new_state));
};

class AssistantManagerServiceImplTest : public testing::Test {
 public:
  AssistantManagerServiceImplTest() = default;

  AssistantManagerServiceImplTest(const AssistantManagerServiceImplTest&) =
      delete;
  AssistantManagerServiceImplTest& operator=(
      const AssistantManagerServiceImplTest&) = delete;

  ~AssistantManagerServiceImplTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->SetTabletMode(
        chromeos::PowerManagerClient::TabletMode::OFF, base::TimeTicks());

    mojo::PendingRemote<device::mojom::BatteryMonitor> battery_monitor;
    delegate_.RequestBatteryMonitor(
        battery_monitor.InitWithNewPipeAndPassReceiver());

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);

    alarm_timer_controller_ =
        std::make_unique<NiceMock<AssistantAlarmTimerControllerMock>>();

    service_context_ = std::make_unique<FakeServiceContext>();
    service_context_
        ->set_main_task_runner(task_environment().GetMainThreadTaskRunner())
        .set_power_manager_client(chromeos::PowerManagerClient::Get())
        .set_assistant_state(&assistant_state_)
        .set_cras_audio_handler(&cras_audio_handler_.Get())
        .set_assistant_alarm_timer_controller(alarm_timer_controller_.get());

    CreateAssistantManagerServiceImpl();

    // Flushes the background thread to let Mojom finish all its work (i.e.
    // binding controllers) before moving formard.
    RunUntilIdle();
  }

  void TearDown() override {
    assistant_manager_service_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  void CreateAssistantManagerServiceImpl(
      std::optional<std::string> s3_server_uri_override = std::nullopt,
      std::optional<std::string> device_id_override = std::nullopt) {
    // We can not have 2 instances of |AssistantManagerServiceImpl| at the same
    // time, so we must destroy the old one before creating a new one.
    assistant_manager_service_.reset();

    assistant_manager_service_ = std::make_unique<AssistantManagerServiceImpl>(
        service_context_.get(), shared_url_loader_factory_->Clone(),
        s3_server_uri_override, device_id_override,
        std::make_unique<FakeLibassistantServiceHost>(&libassistant_service_));
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

  void SetAssistantStateContext(bool enabled) {
    assistant_state_.SetContextEnabled(enabled);
  }

  FakeServiceContext* fake_service_context() { return service_context_.get(); }

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
    FlushForTesting();
    // Then handle any callbacks.
    base::RunLoop().RunUntilIdle();
  }

  void FlushForTesting() {
    libassistant_service_.FlushForTesting();
    background_thread().FlushForTesting();
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

 private:
  base::Thread& background_thread() {
    return assistant_manager_service()->GetBackgroundThreadForTesting();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  ScopedAssistantBrowserDelegate delegate_;
  ScopedCrasAudioHandlerForTesting cras_audio_handler_;
  ScopedDeviceActions device_actions_;
  FullyInitializedAssistantState assistant_state_;

  // Fake implementation of the Libassistant Mojom service.
  FakeLibassistantService libassistant_service_;

  std::unique_ptr<AssistantAlarmTimerControllerMock> alarm_timer_controller_;
  std::unique_ptr<FakeServiceContext> service_context_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<AssistantManagerServiceImpl> assistant_manager_service_;
};

class SpeakerIdEnrollmentControllerMock
    : public libassistant::mojom::SpeakerIdEnrollmentController {
 public:
  SpeakerIdEnrollmentControllerMock() = default;
  SpeakerIdEnrollmentControllerMock(const SpeakerIdEnrollmentControllerMock&) =
      delete;
  SpeakerIdEnrollmentControllerMock& operator=(
      const SpeakerIdEnrollmentControllerMock&) = delete;
  ~SpeakerIdEnrollmentControllerMock() override = default;

  // libassistant::mojom::SpeakerIdEnrollmentController implementation:
  MOCK_METHOD(
      void,
      StartSpeakerIdEnrollment,
      (const std::string& user_gaia_id,
       bool skip_cloud_enrollment,
       mojo::PendingRemote<libassistant::mojom::SpeakerIdEnrollmentClient>
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

TEST_F(AssistantManagerServiceImplTest, ShouldSetStateToDisconnected) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  mojom_service_controller().SetState(ServiceState::kDisconnected);
  WaitForState(AssistantManagerService::DISCONNECTED);
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
       ShouldPassUserInfoToAssistantManagerWhenStarting) {
  assistant_manager_service()->Start(UserInfo("<user-id>", "<access-token>"),
                                     /*enable_hotword=*/false);

  WaitForState(AssistantManagerService::STARTED);

  EXPECT_EQ("<user-id>", mojom_service_controller().gaia_id());
  EXPECT_EQ("<access-token>", mojom_service_controller().access_token());
}

// TODO(crbug.com/40902244): Re-enable this test
TEST_F(AssistantManagerServiceImplTest,
       DISABLED_ShouldPassUserInfoToAssistantManager) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  assistant_manager_service()->SetUser(
      UserInfo("<new-user-id>", "<new-access-token>"));
  RunUntilIdle();

  EXPECT_EQ("<new-user-id>", mojom_service_controller().gaia_id());
  EXPECT_EQ("<new-access-token>", mojom_service_controller().access_token());
}

TEST_F(AssistantManagerServiceImplTest,
       // TODO(crbug.com/40902244): Re-enable this test
       DISABLED_ShouldPassEmptyUserInfoToAssistantManager) {
  Start();
  WaitForState(AssistantManagerService::STARTED);

  assistant_manager_service()->SetUser(std::nullopt);
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
      /*s3_server_uri_override=*/std::nullopt, "the-device-id-override");

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
              OnStateChanged(AssistantManagerService::State::STARTED));

  Start();
  WaitForState(AssistantManagerService::STARTED);

  assistant_manager_service()->AddAndFireStateObserver(&observer);

  assistant_manager_service()->RemoveStateObserver(&observer);
}

TEST_F(AssistantManagerServiceImplTest, ShouldFireStateObserverWhenStarting) {
  StrictMock<StateObserverMock> observer;
  AddStateObserver(&observer);

  mojom_service_controller().BlockStartCalls();

  EXPECT_CALL(observer,
              OnStateChanged(AssistantManagerService::State::STARTING));
  Start();

  assistant_manager_service()->RemoveStateObserver(&observer);
  mojom_service_controller().UnblockStartCalls();
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
              OnStateChanged(AssistantManagerService::State::STOPPING));
  EXPECT_CALL(observer,
              OnStateChanged(AssistantManagerService::State::STOPPED));

  assistant_manager_service()->Stop();
  WaitForState(AssistantManagerService::STOPPING);
  WaitForState(AssistantManagerService::STOPPED);

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
  StartAndWaitForRunning();

  StrictMock<SpeakerIdEnrollmentClientMock> client_mock;
  StrictMock<SpeakerIdEnrollmentControllerMock> mojom_mock;

  mojom_mock.Bind(mojom_libassistant_service());

  EXPECT_CALL(mojom_mock, GetSpeakerIdEnrollmentStatus)
      .WillOnce([](const std::string& user_gaia_id,
                   SpeakerIdEnrollmentControllerMock::
                       GetSpeakerIdEnrollmentStatusCallback callback) {
        std::move(callback).Run(
            SpeakerIdEnrollmentStatus::New(/*user_model_exists=*/true));
      });

  mojom_mock.FlushForTesting();
}

// TODO(crbug.com/40902244): Re-enable this test
TEST_F(AssistantManagerServiceImplTest, DISABLED_ShouldPropagateColorMode) {
  ASSERT_FALSE(mojom_service_controller().dark_mode_enabled().has_value());

  StartAndWaitForRunning();

  ASSERT_TRUE(mojom_service_controller().dark_mode_enabled().has_value());
  EXPECT_FALSE(mojom_service_controller().dark_mode_enabled().value());

  assistant_manager_service()->OnColorModeChanged(true);
  FlushForTesting();

  ASSERT_TRUE(mojom_service_controller().dark_mode_enabled().has_value());
  EXPECT_TRUE(mojom_service_controller().dark_mode_enabled().value());
}

TEST_F(AssistantManagerServiceImplTest, ShouldNotCrashRunningAfterStopped) {
  Start();
  SetAssistantStateContext(/*enabled=*/false);
  WaitForState(AssistantManagerService::STARTED);

  // http://crbug.com/1414264: calling Stop() before Running is set, should not
  // crash.
  assistant_manager_service()->Stop();
  WaitForState(AssistantManagerService::STOPPING);

  mojom_service_controller().SetState(ServiceState::kRunning);
  WaitForState(AssistantManagerService::RUNNING);
}

}  // namespace ash::assistant
