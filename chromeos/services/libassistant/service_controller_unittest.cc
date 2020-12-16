// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/service_controller.h"

#include <memory>

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
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace libassistant {

namespace {

using mojom::ServiceState;
using ::testing::StrictMock;

#define EXPECT_NO_CALLS(args...) EXPECT_CALL(args).Times(0)

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
      OnDestroyingAssistantManager,
      (assistant_client::AssistantManager * assistant_manager,
       assistant_client::AssistantManagerInternal* assistant_manager_internal));
};

class ServiceControllerTest : public testing::Test {
 public:
  ServiceControllerTest()
      : service_controller_(std::make_unique<ServiceController>(

            &delegate_,
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

  void Start(const std::string& libassistant_config = std::string("")) {
    service_controller().Start(libassistant_config);
    RunUntilIdle();
  }

  void Stop() {
    service_controller().Stop();
    RunUntilIdle();
  }

  void DestroyServiceController() { service_controller_.reset(); }

  assistant::LibassistantV1Api& v1_api() {
    return *assistant::LibassistantV1Api::Get();
  }

  assistant::FakeAssistantManagerServiceDelegate& delegate() {
    return delegate_;
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
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

TEST_F(ServiceControllerTest, StateShouldStartAsStopped) {
  StateObserverMock observer;

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStopped));

  AddAndFireStateObserver(&observer);
}

TEST_F(ServiceControllerTest, StateShouldChangeToStartedAfterCallingStart) {
  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStarted));

  Start();
}

TEST_F(ServiceControllerTest, StateShouldChangeToStoppedAfterCallingStop) {
  Start();

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStopped));

  Stop();
}

TEST_F(ServiceControllerTest, ShouldSendCurrentStateWhenAddingObserver) {
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

TEST_F(ServiceControllerTest, ShouldCreateAssistantManagerWhenCallingStart) {
  EXPECT_EQ(nullptr, service_controller().assistant_manager());

  Start();

  EXPECT_EQ(v1_api().assistant_manager(),
            service_controller().assistant_manager());
}

TEST_F(ServiceControllerTest, ShouldBeNoopWhenCallingStartTwice) {
  // Note: This is the prefered behavior for services exposed through mojom.
  Start();

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_NO_CALLS(observer, OnStateChanged);

  Start();
}

TEST_F(ServiceControllerTest, CallingStopTwiceShouldBeANoop) {
  Stop();

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_NO_CALLS(observer, OnStateChanged);

  Stop();
}

TEST_F(ServiceControllerTest, ShouldAllowStartAfgterStop) {
  Start();
  Stop();

  // The second Start() call should create a new |AssistantManager| and send out
  // a state update.

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStarted));

  Start();

  EXPECT_EQ(v1_api().assistant_manager(),
            service_controller().assistant_manager());
}

TEST_F(ServiceControllerTest, ShouldDestroyAssistantManagerWhenCallingStop) {
  Start();
  EXPECT_NE(nullptr, service_controller().assistant_manager());

  Stop();

  EXPECT_EQ(nullptr, service_controller().assistant_manager());
}

TEST_F(ServiceControllerTest, StateShouldChangeToStoppedWhenBeingDestroyed) {
  Start();

  StateObserverMock observer;
  AddStateObserver(&observer);

  EXPECT_CALL(observer, OnStateChanged(ServiceState::kStopped));

  DestroyServiceController();
  RunUntilIdle();
}

TEST_F(ServiceControllerTest,
       ShouldCreateAssistantManagerInternalWhenCallingStart) {
  EXPECT_EQ(nullptr, service_controller().assistant_manager_internal());

  Start();

  EXPECT_EQ(v1_api().assistant_manager_internal(),
            service_controller().assistant_manager_internal());
}

TEST_F(ServiceControllerTest,
       ShouldDestroyAssistantManagerInternalWhenCallingStop) {
  Start();
  EXPECT_NE(nullptr, service_controller().assistant_manager_internal());

  Stop();

  EXPECT_EQ(nullptr, service_controller().assistant_manager_internal());
}

TEST_F(ServiceControllerTest, ShouldPassLibassistantConfigToAssistantManager) {
  Start(/*libassistant_config=*/"the-libassistant-config");

  EXPECT_EQ("the-libassistant-config", delegate().libassistant_config());
}

TEST_F(ServiceControllerTest,
       ShouldCallOnAssistantManagerCreatedWhenCallingStart) {
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

  Start();

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(ServiceControllerTest,
       ShouldCallOnAssistantManagerCreatedWhenAddingObserver) {
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

  AddAndFireAssistantManagerObserver(&observer);

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(ServiceControllerTest,
       ShouldCallOnDestroyingAssistantManagerWhenCallingStop) {
  StrictMock<AssistantManagerObserverMock> observer;
  AddAndFireAssistantManagerObserver(&observer);

  EXPECT_CALL(observer, OnAssistantManagerCreated);
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

  Stop();

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(ServiceControllerTest,
       ShouldNotCallAssistantManagerObserverWhenItHasBeenRemoved) {
  StrictMock<AssistantManagerObserverMock> observer;
  AddAndFireAssistantManagerObserver(&observer);
  RemoveAssistantManagerObserver(&observer);

  EXPECT_NO_CALLS(observer, OnAssistantManagerCreated);
  EXPECT_NO_CALLS(observer, OnDestroyingAssistantManager);

  Start();
  Stop();

  RemoveAssistantManagerObserver(&observer);
}

TEST_F(ServiceControllerTest,
       ShouldCallOnDestroyingAssistantManagerWhenBeingDestroyed) {
  Start();

  StrictMock<AssistantManagerObserverMock> observer;
  EXPECT_CALL(observer, OnAssistantManagerCreated);
  AddAndFireAssistantManagerObserver(&observer);

  EXPECT_CALL(observer, OnDestroyingAssistantManager);
  DestroyServiceController();
}

}  // namespace libassistant
}  // namespace chromeos
