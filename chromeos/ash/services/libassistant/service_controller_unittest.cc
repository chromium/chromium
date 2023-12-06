// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/service_controller.h"

#include <memory>

#include "base/base_paths.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/ash/services/libassistant/public/mojom/service_controller.mojom.h"
#include "chromeos/ash/services/libassistant/public/mojom/settings_controller.mojom.h"
#include "chromeos/ash/services/libassistant/settings_controller.h"
#include "chromeos/ash/services/libassistant/test_support/fake_libassistant_factory.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::libassistant {

namespace {

using mojom::ServiceState;
using ::testing::StrictMock;

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0)

// Tests if the JSON string contains the given path with the given value.
#define EXPECT_HAS_PATH_WITH_VALUE(config_string, path, expected_value)        \
  ({                                                                           \
    std::optional<base::Value> config = base::JSONReader::Read(config_string); \
    ASSERT_TRUE(config.has_value());                                           \
    ASSERT_TRUE(config->is_dict());                                            \
    const base::Value* actual = config->GetDict().FindByDottedPath(path);      \
    base::Value expected = base::Value(expected_value);                        \
    ASSERT_NE(actual, nullptr)                                                 \
        << "Path '" << path << "' not found in config: " << config_string;     \
    EXPECT_EQ(*actual, expected);                                              \
  })

std::vector<mojom::AuthenticationTokenPtr> ToVector(
    mojom::AuthenticationTokenPtr token) {
  std::vector<mojom::AuthenticationTokenPtr> result;
  result.push_back(std::move(token));
  return result;
}

class StateObserverMock : public mojom::StateObserver {
 public:
  StateObserverMock() : receiver_(this) {}

  MOCK_METHOD(void, OnStateChanged, (ServiceState));

  mojo::PendingRemote<mojom::StateObserver> BindAndPassReceiver() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<mojom::StateObserver> receiver_;
};

class AssistantClientObserverMock : public AssistantClientObserver {
 public:
  AssistantClientObserverMock() = default;
  AssistantClientObserverMock(const AssistantClientObserverMock&) = delete;
  AssistantClientObserverMock& operator=(const AssistantClientObserverMock&) =
      delete;
  ~AssistantClientObserverMock() override = default;

  // AssistantClientObserver implementation:
  MOCK_METHOD(void,
              OnAssistantClientCreated,
              (AssistantClient * assistant_client));
  MOCK_METHOD(void,
              OnAssistantClientStarted,
              (AssistantClient * assistant_client));
  MOCK_METHOD(void,
              OnAssistantClientRunning,
              (AssistantClient * assistant_client));
  MOCK_METHOD(void,
              OnDestroyingAssistantClient,
              (AssistantClient * assistant_client));
  MOCK_METHOD(void, OnAssistantClientDestroyed, ());
};

class SettingsControllerMock : public mojom::SettingsController {
 public:
  SettingsControllerMock() = default;
  SettingsControllerMock(const SettingsControllerMock&) = delete;
  SettingsControllerMock& operator=(const SettingsControllerMock&) = delete;
  ~SettingsControllerMock() override = default;

  // mojom::SettingsController implementation:
  MOCK_METHOD(void,
              SetAuthenticationTokens,
              (std::vector<mojom::AuthenticationTokenPtr> tokens));
  MOCK_METHOD(void, SetListeningEnabled, (bool value));
  MOCK_METHOD(void, SetLocale, (const std::string& value));
  MOCK_METHOD(void, SetSpokenFeedbackEnabled, (bool value));
  MOCK_METHOD(void, SetDarkModeEnabled, (bool value));
  MOCK_METHOD(void, SetHotwordEnabled, (bool value));
  MOCK_METHOD(void,
              GetSettings,
              (const std::string& selector,
               bool include_header,
               GetSettingsCallback callback));
  MOCK_METHOD(void,
              UpdateSettings,
              (const std::string& settings, UpdateSettingsCallback callback));
};

class MediaManagerMock : public assistant_client::MediaManager {
 public:
  MediaManagerMock() = default;
  MediaManagerMock(const MediaManagerMock&) = delete;
  MediaManagerMock& operator=(const MediaManagerMock&) = delete;
  ~MediaManagerMock() override = default;

  // assistant_client::MediaManager:
  MOCK_METHOD(void, AddListener, (Listener * listener));
  MOCK_METHOD(void, Next, ());
  MOCK_METHOD(void, Previous, ());
  MOCK_METHOD(void, Resume, ());
  MOCK_METHOD(void, Pause, ());
  MOCK_METHOD(void, PlayPause, ());
  MOCK_METHOD(void, StopAndClearPlaylist, ());
  MOCK_METHOD(void,
              SetExternalPlaybackState,
              (const assistant_client::MediaStatus& new_status));
};

class AssistantServiceControllerTest : public testing::Test {
 public:
  AssistantServiceControllerTest()
      : service_controller_(
            std::make_unique<ServiceController>(&libassistant_factory_)) {
    service_controller_->Bind(client_.BindNewPipeAndPassReceiver(),
                              &settings_controller_);
  }

  mojo::Remote<mojom::ServiceController>& client() { return client_; }
  ServiceController& service_controller() {
    DCHECK(service_controller_);
    return *service_controller_;
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  // Add the state observer. Will expect the call that follows immediately after
  // adding the observer.
  void AddStateObserver(StateObserverMock* observer) {
    EXPECT_CALL(*observer, OnStateChanged);
    service_controller().AddAndFireStateObserver(
        observer->BindAndPassReceiver());
    RunUntilIdle();
  }

  void AddAndFireAssistantClientObserver(AssistantClientObserver* observer) {
    service_controller().AddAndFireAssistantClientObserver(observer);
  }

  void RemoveAssistantClientObserver(AssistantClientObserver* observer) {
    service_controller().RemoveAssistantClientObserver(observer);
  }

  void AddAndFireStateObserver(StateObserverMock* observer) {
    service_controller().AddAndFireStateObserver(
        observer->BindAndPassReceiver());
    RunUntilIdle();
  }

  void Initialize(mojom::BootupConfigPtr config = mojom::BootupConfig::New()) {
    service_controller().Initialize(std::move(config), BindURLLoaderFactory());
  }

  void Start() {
    service_controller().Start();
    libassistant_factory_.assistant_manager().SetMediaManager(&media_manager_);

    // Similuate gRPC heartbeat calls.
    service_controller().OnServicesStatusChanged(
        ServicesStatus::ONLINE_BOOTING_UP);
    RunUntilIdle();
  }

  void SendOnStartFinished() {
    // Similuate gRPC heartbeat calls.
    service_controller().OnServicesStatusChanged(
        ServicesStatus::ONLINE_ALL_SERVICES_AVAILABLE);
    RunUntilIdle();
  }

  void Stop() {
    service_controller().Stop();
    RunUntilIdle();
  }

  void DestroyServiceController() { service_controller_.reset(); }

  std::string libassistant_config() {
    return libassistant_factory_.libassistant_config();
  }

  SettingsControllerMock& settings_controller_mock() {
    return settings_controller_;
  }

 private:
  mojo::PendingRemote<network::mojom::URLLoaderFactory> BindURLLoaderFactory() {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
    url_loader_factory_.Clone(pending_remote.InitWithNewPipeAndPassReceiver());
    return pending_remote;
  }

  base::test::SingleThreadTaskEnvironment environment_;
  base::ScopedPathOverride home_override{base::DIR_HOME};

  network::TestURLLoaderFactory url_loader_factory_;

  FakeLibassistantFactory libassistant_factory_;
  testing::NiceMock<SettingsControllerMock> settings_controller_;
  mojo::Remote<mojom::ServiceController> client_;
  std::unique_ptr<ServiceController> service_controller_;
  MediaManagerMock media_manager_;
};

}  // namespace

namespace mojom {

void PrintTo(const ServiceState state, std::ostream* stream) {
  switch (state) {
    case ServiceState::kRunning:
      *stream << "kRunning";
      return;
    case ServiceState::kStarted:
      *stream << "kStarted";
      return;
    case ServiceState::kStopped:
      *stream << "kStopped";
      return;
    case ServiceState::kDisconnected:
      *stream << "kDisconnected";
      return;
  }
  *stream << "INVALID ServiceState (" << static_cast<int>(state) << ")";
}

}  // namespace mojom

TEST_F(AssistantServiceControllerTest, StateShouldStartAsStopped) {
  Initialize();
  StateObserverMock observer;

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStopped));

  AddAndFireStateObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       StateShouldChangeToStartedAfterCallingStart) {
  Initialize();
  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStarted));

  Start();
}

TEST_F(AssistantServiceControllerTest,
       StateShouldChangeToStoppedAfterCallingStop) {
  Initialize();
  Start();

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStopped));

  Stop();
}

TEST_F(AssistantServiceControllerTest,
       StateShouldChangeToRunningAfterLibassistantSignalsItsDone) {
  Initialize();
  Start();

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kRunning));

  SendOnStartFinished();
}

TEST_F(AssistantServiceControllerTest,
       ShouldSendCurrentStateWhenAddingObserver) {
  Initialize();

  {
    StateObserverMock observer;

    EXPECT_CALL(observer, OnStateChanged(ServiceState::kStopped));
    AddAndFireStateObserver(&observer);
  }

  Start();

  {
    StateObserverMock observer;

    EXPECT_CALL(observer, OnStateChanged(ServiceState::kStarted));
    AddAndFireStateObserver(&observer);
  }

  Stop();

  {
    StateObserverMock observer;

    EXPECT_CALL(observer, OnStateChanged(ServiceState::kStopped));
    AddAndFireStateObserver(&observer);
  }
}

TEST_F(AssistantServiceControllerTest,
       ShouldCreateAssistantManagerWhenCallingInitialize) {
  EXPECT_EQ(nullptr, service_controller().assistant_client());

  Initialize();

  EXPECT_NE(nullptr,
            service_controller().assistant_client()->assistant_manager());
}

TEST_F(AssistantServiceControllerTest, CallingStopTwiceShouldBeANoop) {
  Initialize();
  Stop();

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_NO_CALLS(observer, OnStateChanged);

  Stop();
}

TEST_F(AssistantServiceControllerTest, ShouldAllowStartAfterStop) {
  Initialize();
  Start();
  Stop();

  // The second Initialize() call should create the AssistantManager.

  Initialize();
  EXPECT_NE(nullptr,
            service_controller().assistant_client()->assistant_manager());

  // The second Start() call should send out a state update.

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStarted));

  Start();

  EXPECT_NE(nullptr,
            service_controller().assistant_client()->assistant_manager());
}

TEST_F(AssistantServiceControllerTest,
       ShouldDestroyAssistantManagerWhenCallingStop) {
  Initialize();
  Start();
  EXPECT_NE(nullptr,
            service_controller().assistant_client()->assistant_manager());

  Stop();

  EXPECT_EQ(nullptr, service_controller().assistant_client());
}

TEST_F(AssistantServiceControllerTest,
       StateShouldChangeToStoppedWhenBeingDestroyed) {
  Initialize();
  Start();

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStopped));

  DestroyServiceController();
  RunUntilIdle();
}

TEST_F(AssistantServiceControllerTest,
       ShouldPassS3ServerUriOverrideToMojomService) {
  auto bootup_config = mojom::BootupConfig::New();
  bootup_config->s3_server_uri_override = "the-s3-server-uri-override";
  Initialize(std::move(bootup_config));

  EXPECT_HAS_PATH_WITH_VALUE(libassistant_config(),
                             "testing.s3_grpc_server_uri",
                             "the-s3-server-uri-override");
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantClientCreatedWhenCallingInitialize) {
  StrictMock<AssistantClientObserverMock> observer;
  AddAndFireAssistantClientObserver(&observer);

  EXPECT_CALL(observer, OnAssistantClientCreated)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });

  Initialize();

  RemoveAssistantClientObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantClientCreatedWhenAddingObserver) {
  Initialize();

  StrictMock<AssistantClientObserverMock> observer;

  EXPECT_CALL(observer, OnAssistantClientCreated)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });

  AddAndFireAssistantClientObserver(&observer);

  RemoveAssistantClientObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantClientStartedWhenCallingStart) {
  StrictMock<AssistantClientObserverMock> observer;
  AddAndFireAssistantClientObserver(&observer);

  EXPECT_CALL(observer, OnAssistantClientCreated);
  Initialize();

  EXPECT_CALL(observer, OnAssistantClientStarted)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });

  Start();

  RemoveAssistantClientObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantManagerInitializedAndCreatedWhenAddingObserver) {
  Initialize();
  Start();

  StrictMock<AssistantClientObserverMock> observer;

  EXPECT_CALL(observer, OnAssistantClientCreated)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });

  EXPECT_CALL(observer, OnAssistantClientStarted)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });

  AddAndFireAssistantClientObserver(&observer);

  RemoveAssistantClientObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantClientRunningWhenCallingOnStartFinished) {
  StrictMock<AssistantClientObserverMock> observer;
  AddAndFireAssistantClientObserver(&observer);

  EXPECT_CALL(observer, OnAssistantClientCreated);
  Initialize();
  EXPECT_CALL(observer, OnAssistantClientStarted);
  Start();

  EXPECT_CALL(observer, OnAssistantClientRunning)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });

  SendOnStartFinished();

  RemoveAssistantClientObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantClientRunningWhenAddingObserver) {
  Initialize();
  Start();
  SendOnStartFinished();

  StrictMock<AssistantClientObserverMock> observer;

  EXPECT_CALL(observer, OnAssistantClientCreated)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });

  EXPECT_CALL(observer, OnAssistantClientStarted)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });

  EXPECT_CALL(observer, OnAssistantClientRunning)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });

  AddAndFireAssistantClientObserver(&observer);

  RemoveAssistantClientObserver(&observer);
}
TEST_F(AssistantServiceControllerTest,
       ShouldCallOnDestroyingAssistantClientWhenCallingStop) {
  StrictMock<AssistantClientObserverMock> observer;
  AddAndFireAssistantClientObserver(&observer);

  EXPECT_CALL(observer, OnAssistantClientCreated);
  EXPECT_CALL(observer, OnAssistantClientStarted);
  Initialize();
  Start();

  EXPECT_CALL(observer, OnDestroyingAssistantClient)
      .WillOnce([&controller =
                     service_controller()](AssistantClient* assistant_client) {
        EXPECT_EQ(assistant_client, controller.assistant_client());
      });
  EXPECT_CALL(observer, OnAssistantClientDestroyed);

  Stop();

  RemoveAssistantClientObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldNotCallAssistantClientObserverWhenItHasBeenRemoved) {
  StrictMock<AssistantClientObserverMock> observer;
  AddAndFireAssistantClientObserver(&observer);
  RemoveAssistantClientObserver(&observer);

  EXPECT_NO_CALLS(observer, OnAssistantClientCreated);
  EXPECT_NO_CALLS(observer, OnAssistantClientStarted);
  EXPECT_NO_CALLS(observer, OnDestroyingAssistantClient);
  EXPECT_NO_CALLS(observer, OnAssistantClientDestroyed);

  Initialize();
  Start();
  Stop();

  RemoveAssistantClientObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnDestroyingAssistantClientWhenBeingDestroyed) {
  Initialize();
  Start();

  StrictMock<AssistantClientObserverMock> observer;
  EXPECT_CALL(observer, OnAssistantClientCreated);
  EXPECT_CALL(observer, OnAssistantClientStarted);
  AddAndFireAssistantClientObserver(&observer);

  EXPECT_CALL(observer, OnDestroyingAssistantClient);
  EXPECT_CALL(observer, OnAssistantClientDestroyed);
  DestroyServiceController();
}

TEST_F(AssistantServiceControllerTest,
       ShouldPassBootupConfigToSettingsController) {
  const bool hotword_enabled = true;
  const bool spoken_feedback_enabled = false;

  EXPECT_CALL(settings_controller_mock(), SetLocale("locale"));
  EXPECT_CALL(settings_controller_mock(), SetHotwordEnabled(hotword_enabled));
  EXPECT_CALL(settings_controller_mock(),
              SetSpokenFeedbackEnabled(spoken_feedback_enabled));
  EXPECT_CALL(settings_controller_mock(), SetAuthenticationTokens);

  auto bootup_config = mojom::BootupConfig::New();
  bootup_config->locale = "locale";
  bootup_config->hotword_enabled = hotword_enabled;
  bootup_config->spoken_feedback_enabled = spoken_feedback_enabled;
  bootup_config->authentication_tokens =
      ToVector(mojom::AuthenticationToken::New("user", "token"));

  Initialize(std::move(bootup_config));
}

}  // namespace ash::libassistant
