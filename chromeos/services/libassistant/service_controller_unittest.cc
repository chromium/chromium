// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/service_controller.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/assistant//public/cpp/migration/fake_assistant_manager_service_delegate.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "chromeos/services/libassistant/assistant_manager_observer.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/device_state_listener.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace libassistant {

namespace {

using mojom::ServiceState;
using ::testing::StrictMock;

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0)

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

class AssistantManagerObserverMock : public AssistantManagerObserver {
 public:
  AssistantManagerObserverMock() = default;
  AssistantManagerObserverMock(AssistantManagerObserverMock&) = delete;
  AssistantManagerObserverMock& operator=(AssistantManagerObserverMock&) =
      delete;
  ~AssistantManagerObserverMock() override = default;

  // AssistantManagerObserver implementation:
  MOCK_METHOD(
      void,
      OnAssistantManagerCreated,
      (assistant_client::AssistantManager * assistant_manager,
       assistant_client::AssistantManagerInternal* assistant_manager_internal));
  MOCK_METHOD(
      void,
      OnAssistantManagerStarted,
      (assistant_client::AssistantManager * assistant_manager,
       assistant_client::AssistantManagerInternal* assistant_manager_internal));
  MOCK_METHOD(
      void,
      OnAssistantManagerRunning,
      (assistant_client::AssistantManager * assistant_manager,
       assistant_client::AssistantManagerInternal* assistant_manager_internal));
  MOCK_METHOD(
      void,
      OnDestroyingAssistantManager,
      (assistant_client::AssistantManager * assistant_manager,
       assistant_client::AssistantManagerInternal* assistant_manager_internal));
  MOCK_METHOD(void, OnAssistantManagerDestroyed, ());
};

class AssistantServiceControllerTest : public testing::Test {
 public:
  AssistantServiceControllerTest()
      : service_controller_(
            std::make_unique<ServiceController>(&delegate_,
                                                /*platform_api=*/nullptr)) {
    service_controller_->Bind(client_.BindNewPipeAndPassReceiver());
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

  void AddAndFireAssistantManagerObserver(AssistantManagerObserver* observer) {
    service_controller().AddAndFireAssistantManagerObserver(observer);
  }

  void RemoveAssistantManagerObserver(AssistantManagerObserver* observer) {
    service_controller().RemoveAssistantManagerObserver(observer);
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
    RunUntilIdle();
  }

  void SendOnStartFinished() {
    auto* device_state_listener =
        delegate().assistant_manager()->device_state_listener();
    ASSERT_NE(device_state_listener, nullptr);
    device_state_listener->OnStartFinished();
    RunUntilIdle();
  }

  void Stop() {
    service_controller().Stop();
    RunUntilIdle();
  }

  void DestroyServiceController() { service_controller_.reset(); }

  assistant::LibassistantV1Api* v1_api() {
    return assistant::LibassistantV1Api::Get();
  }

  assistant::FakeAssistantManagerServiceDelegate& delegate() {
    return delegate_;
  }

 private:
  mojo::PendingRemote<network::mojom::URLLoaderFactory> BindURLLoaderFactory() {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
    url_loader_factory_.Clone(pending_remote.InitWithNewPipeAndPassReceiver());
    return pending_remote;
  }

  base::test::SingleThreadTaskEnvironment environment_;

  network::TestURLLoaderFactory url_loader_factory_;

  assistant::FakeAssistantManagerServiceDelegate delegate_;
  mojo::Remote<mojom::ServiceController> client_;
  std::unique_ptr<ServiceController> service_controller_;
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
  EXPECT_EQ(nullptr, service_controller().assistant_manager());

  Initialize();

  EXPECT_NE(nullptr, service_controller().assistant_manager());
}

TEST_F(AssistantServiceControllerTest, ShouldBeNoopWhenCallingStartTwice) {
  // Note: This is the preferred behavior for services exposed through mojom.
  Initialize();
  Start();

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_NO_CALLS(observer, OnStateChanged);

  Start();
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

  // The second Initialize() call should create the AssistantManager and
  // LibassistantV1Api.

  Initialize();
  EXPECT_NE(nullptr, service_controller().assistant_manager());
  EXPECT_NE(nullptr, v1_api());

  // The second Start() call should send out a state update and publish the
  // v1_api

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStarted));

  Start();

  ASSERT_NE(nullptr, v1_api());
  EXPECT_NE(nullptr, service_controller().assistant_manager());
  EXPECT_EQ(v1_api()->assistant_manager(),
            service_controller().assistant_manager());
}

TEST_F(AssistantServiceControllerTest,
       ShouldDestroyAssistantManagerWhenCallingStop) {
  Initialize();
  Start();
  EXPECT_NE(nullptr, service_controller().assistant_manager());

  Stop();

  EXPECT_EQ(nullptr, service_controller().assistant_manager());
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
       ShouldCreateButNotPublishAssistantManagerInternalWhenCallingInitialize) {
  EXPECT_EQ(nullptr, service_controller().assistant_manager_internal());

  Initialize();

  EXPECT_NE(nullptr, service_controller().assistant_manager_internal());
  EXPECT_NE(nullptr, v1_api());
}

TEST_F(AssistantServiceControllerTest,
       ShouldPublishAssistantManagerInternalWhenCallingStart) {
  Initialize();
  Start();

  ASSERT_NE(nullptr, v1_api());
  EXPECT_NE(nullptr, service_controller().assistant_manager_internal());
  EXPECT_EQ(v1_api()->assistant_manager_internal(),
            service_controller().assistant_manager_internal());
}

TEST_F(AssistantServiceControllerTest,
       ShouldDestroyAssistantManagerInternalWhenCallingStop) {
  Initialize();

  EXPECT_NE(nullptr, service_controller().assistant_manager_internal());

  Start();
  Stop();

  EXPECT_EQ(nullptr, service_controller().assistant_manager_internal());
  EXPECT_EQ(nullptr, v1_api());
}

TEST_F(AssistantServiceControllerTest,
       ShouldPassS3ServerUriOverrideToMojomService) {
  auto bootup_config = mojom::BootupConfig::New();
  bootup_config->s3_server_uri_override = "the-s3-server-uri-override";
  Initialize(std::move(bootup_config));

  EXPECT_HAS_PATH_WITH_VALUE(delegate().libassistant_config(),
                             "testing.s3_grpc_server_uri",
                             "the-s3-server-uri-override");
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantManagerCreatedWhenCallingInitialize) {
  StrictMock<AssistantManagerObserverMock> observer;
  AddAndFireAssistantManagerObserver(&observer);

  EXPECT_CALL(observer, OnAssistantManagerCreated)
      .WillOnce([&controller = service_controller()](
                    assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, controller.assistant_manager());
        EXPECT_EQ(assistant_manager_internal,
                  controller.assistant_manager_internal());
      });

  Initialize();

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantManagerCreatedWhenAddingObserver) {
  Initialize();

  StrictMock<AssistantManagerObserverMock> observer;

  EXPECT_CALL(observer, OnAssistantManagerCreated)
      .WillOnce([&controller = service_controller()](
                    assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, controller.assistant_manager());
        EXPECT_EQ(assistant_manager_internal,
                  controller.assistant_manager_internal());
      });

  AddAndFireAssistantManagerObserver(&observer);

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantManagerStartedWhenCallingStart) {
  StrictMock<AssistantManagerObserverMock> observer;
  AddAndFireAssistantManagerObserver(&observer);

  EXPECT_CALL(observer, OnAssistantManagerCreated);
  Initialize();

  EXPECT_CALL(observer, OnAssistantManagerStarted)
      .WillOnce([&controller = service_controller()](
                    assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, controller.assistant_manager());
        EXPECT_EQ(assistant_manager_internal,
                  controller.assistant_manager_internal());
      });

  Start();

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantManagerInitializedAndCreatedWhenAddingObserver) {
  Initialize();
  Start();

  StrictMock<AssistantManagerObserverMock> observer;

  EXPECT_CALL(observer, OnAssistantManagerCreated)
      .WillOnce([&controller = service_controller()](
                    assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, controller.assistant_manager());
        EXPECT_EQ(assistant_manager_internal,
                  controller.assistant_manager_internal());
      });

  EXPECT_CALL(observer, OnAssistantManagerStarted)
      .WillOnce([&controller = service_controller()](
                    assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, controller.assistant_manager());
        EXPECT_EQ(assistant_manager_internal,
                  controller.assistant_manager_internal());
      });

  AddAndFireAssistantManagerObserver(&observer);

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantManagerRunningWhenCallingOnStartFinished) {
  StrictMock<AssistantManagerObserverMock> observer;
  AddAndFireAssistantManagerObserver(&observer);

  EXPECT_CALL(observer, OnAssistantManagerCreated);
  Initialize();
  EXPECT_CALL(observer, OnAssistantManagerStarted);
  Start();

  EXPECT_CALL(observer, OnAssistantManagerRunning)
      .WillOnce([&controller = service_controller()](
                    assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, controller.assistant_manager());
        EXPECT_EQ(assistant_manager_internal,
                  controller.assistant_manager_internal());
      });

  SendOnStartFinished();

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnAssistantManagerRunningWhenAddingObserver) {
  Initialize();
  Start();
  SendOnStartFinished();

  StrictMock<AssistantManagerObserverMock> observer;

  EXPECT_CALL(observer, OnAssistantManagerCreated)
      .WillOnce([&controller = service_controller()](
                    assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, controller.assistant_manager());
        EXPECT_EQ(assistant_manager_internal,
                  controller.assistant_manager_internal());
      });

  EXPECT_CALL(observer, OnAssistantManagerStarted)
      .WillOnce([&controller = service_controller()](
                    assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, controller.assistant_manager());
        EXPECT_EQ(assistant_manager_internal,
                  controller.assistant_manager_internal());
      });

  EXPECT_CALL(observer, OnAssistantManagerRunning)
      .WillOnce([&controller = service_controller()](
                    assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, controller.assistant_manager());
        EXPECT_EQ(assistant_manager_internal,
                  controller.assistant_manager_internal());
      });

  AddAndFireAssistantManagerObserver(&observer);

  RemoveAssistantManagerObserver(&observer);
}
TEST_F(AssistantServiceControllerTest,
       ShouldCallOnDestroyingAssistantManagerWhenCallingStop) {
  StrictMock<AssistantManagerObserverMock> observer;
  AddAndFireAssistantManagerObserver(&observer);

  EXPECT_CALL(observer, OnAssistantManagerCreated);
  EXPECT_CALL(observer, OnAssistantManagerStarted);
  Initialize();
  Start();

  const auto* expected_assistant_manager =
      service_controller().assistant_manager();
  const auto* expected_assistant_manager_internal =
      service_controller().assistant_manager_internal();

  EXPECT_CALL(observer, OnDestroyingAssistantManager)
      .WillOnce([&](assistant_client::AssistantManager* assistant_manager,
                    assistant_client::AssistantManagerInternal*
                        assistant_manager_internal) {
        EXPECT_EQ(assistant_manager, expected_assistant_manager);
        EXPECT_EQ(assistant_manager_internal,
                  expected_assistant_manager_internal);
      });
  EXPECT_CALL(observer, OnAssistantManagerDestroyed);

  Stop();

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldNotCallAssistantManagerObserverWhenItHasBeenRemoved) {
  StrictMock<AssistantManagerObserverMock> observer;
  AddAndFireAssistantManagerObserver(&observer);
  RemoveAssistantManagerObserver(&observer);

  EXPECT_NO_CALLS(observer, OnAssistantManagerCreated);
  EXPECT_NO_CALLS(observer, OnAssistantManagerStarted);
  EXPECT_NO_CALLS(observer, OnDestroyingAssistantManager);
  EXPECT_NO_CALLS(observer, OnAssistantManagerDestroyed);

  Initialize();
  Start();
  Stop();

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(AssistantServiceControllerTest,
       ShouldCallOnDestroyingAssistantManagerWhenBeingDestroyed) {
  Initialize();
  Start();

  StrictMock<AssistantManagerObserverMock> observer;
  EXPECT_CALL(observer, OnAssistantManagerCreated);
  EXPECT_CALL(observer, OnAssistantManagerStarted);
  AddAndFireAssistantManagerObserver(&observer);

  EXPECT_CALL(observer, OnDestroyingAssistantManager);
  EXPECT_CALL(observer, OnAssistantManagerDestroyed);
  DestroyServiceController();
}

}  // namespace libassistant
}  // namespace chromeos
