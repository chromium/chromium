// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/rmad/rmad_client.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

namespace {

// Matcher that verifies that a dbus::Message has member |name|.
MATCHER_P(HasMember, name, "") {
  if (arg->GetMember() != name) {
    *result_listener << "has member " << arg->GetMember();
    return false;
  }
  return true;
}

}  // namespace

class RmadClientTest : public testing::Test {
 public:
  RmadClientTest() = default;
  RmadClientTest(const RmadClientTest&) = delete;
  RmadClientTest& operator=(const RmadClientTest&) = delete;

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock lorgnette daemon proxy.
    mock_proxy_ =
        new dbus::MockObjectProxy(mock_bus_.get(), rmad::kRmadInterfaceName,
                                  dbus::ObjectPath(rmad::kRmadServicePath));

    // |client_|'s Init() method should request a proxy for communicating with
    // the lorgnette daemon.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(rmad::kRmadInterfaceName,
                               dbus::ObjectPath(rmad::kRmadServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create a client with the mock bus.
    RmadClient::Initialize(mock_bus_.get());
    client_ = RmadClient::Get();
  }

  void TearDown() override {
    mock_bus_->ShutdownAndBlock();
    RmadClient::Shutdown();
  }

  // Responsible for responding to a kListScannersMethod call.
  void OnCallDbusMethod(dbus::MethodCall* method_call,
                        int timeout_ms,
                        dbus::ObjectProxy::ResponseCallback* callback) {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response_));
  }

  RmadClient* client_ = nullptr;  // Unowned convenience pointer.
  // A message loop to emulate asynchronous behavior.
  base::test::SingleThreadTaskEnvironment task_environment_;
  dbus::Response* response_ = nullptr;
  // Mock D-Bus objects for |client_| to interact with.
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
};

TEST_F(RmadClientTest, GetCurrentState) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::GetStateReply expected_proto;
  expected_proto.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetCurrentStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->GetCurrentState(base::BindLambdaForTesting(
      [&](absl::optional<rmad::GetStateReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
        EXPECT_FALSE(response->has_state());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, GetCurrentState_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetCurrentStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->GetCurrentState(base::BindLambdaForTesting(
      [&](absl::optional<rmad::GetStateReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, GetCurrentState_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kGetCurrentStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->GetCurrentState(base::BindLambdaForTesting(
      [&](absl::optional<rmad::GetStateReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionNextState) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::GetStateReply expected_proto;
  rmad::RmadState* expected_state = new rmad::RmadState();
  expected_state->set_allocated_select_network(new rmad::SelectNetworkState());
  expected_proto.set_allocated_state(expected_state);
  expected_proto.set_error(rmad::RMAD_ERROR_OK);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionNextStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  rmad::RmadState request;
  request.set_allocated_welcome(new rmad::WelcomeState());

  base::RunLoop run_loop;
  client_->TransitionNextState(
      request, base::BindLambdaForTesting(
                   [&](absl::optional<rmad::GetStateReply> response) {
                     EXPECT_TRUE(response.has_value());
                     EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
                     EXPECT_TRUE(response->has_state());
                     EXPECT_TRUE(response->state().has_select_network());
                     run_loop.Quit();
                   }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionNextState_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionNextStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  rmad::RmadState request;
  request.set_allocated_welcome(new rmad::WelcomeState());
  base::RunLoop run_loop;
  client_->TransitionNextState(
      request, base::BindLambdaForTesting(
                   [&](absl::optional<rmad::GetStateReply> response) {
                     EXPECT_FALSE(response.has_value());
                     run_loop.Quit();
                   }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionNextState_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionNextStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  rmad::RmadState request;
  request.set_allocated_welcome(new rmad::WelcomeState());
  base::RunLoop run_loop;
  client_->TransitionNextState(
      request, base::BindLambdaForTesting(
                   [&](absl::optional<rmad::GetStateReply> response) {
                     EXPECT_FALSE(response.has_value());
                     run_loop.Quit();
                   }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionPreviousState) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::GetStateReply expected_proto;
  rmad::RmadState* expected_state = new rmad::RmadState();
  expected_state->set_allocated_welcome(new rmad::WelcomeState());
  expected_proto.set_allocated_state(expected_state);
  expected_proto.set_error(rmad::RMAD_ERROR_TRANSITION_FAILED);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionPreviousStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](absl::optional<rmad::GetStateReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_TRANSITION_FAILED);
        EXPECT_TRUE(response->has_state());
        EXPECT_TRUE(response->state().has_welcome());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionPreviousState_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionPreviousStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](absl::optional<rmad::GetStateReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, TransitionPreviousState_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kTransitionPreviousStateMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](absl::optional<rmad::GetStateReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, AbortRma) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  rmad::AbortRmaReply expected_proto;
  expected_proto.set_error(rmad::RMAD_ERROR_OK);
  ASSERT_TRUE(dbus::MessageWriter(response.get())
                  .AppendProtoAsArrayOfBytes(expected_proto));

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kAbortRmaMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->AbortRma(base::BindLambdaForTesting(
      [&](absl::optional<rmad::AbortRmaReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, AbortRma_NullResponse) {
  response_ = nullptr;
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kAbortRmaMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->AbortRma(base::BindLambdaForTesting(
      [&](absl::optional<rmad::AbortRmaReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(RmadClientTest, AbortRma_EmptyResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();

  response_ = response.get();
  EXPECT_CALL(*mock_proxy_.get(),
              DoCallMethod(HasMember(rmad::kAbortRmaMethod),
                           dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, _))
      .WillOnce(Invoke(this, &RmadClientTest::OnCallDbusMethod));

  base::RunLoop run_loop;
  client_->AbortRma(base::BindLambdaForTesting(
      [&](absl::optional<rmad::AbortRmaReply> response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

}  // namespace chromeos
