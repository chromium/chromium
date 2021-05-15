// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/rmad/fake_rmad_client.h"

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
class FakeRmadClientTest : public testing::Test {
 public:
  FakeRmadClientTest() = default;
  FakeRmadClientTest(const FakeRmadClientTest&) = delete;
  FakeRmadClientTest& operator=(const FakeRmadClientTest&) = delete;

  void SetUp() override {
    // Create a client with the mock bus.
    RmadClient::InitializeFake();
    client_ = RmadClient::Get();
  }

  void TearDown() override { RmadClient::Shutdown(); }

  FakeRmadClient* fake_client_() {
    return google::protobuf::down_cast<FakeRmadClient*>(client_);
  }

  RmadClient* client_ = nullptr;  // Unowned convenience pointer.
  // A message loop to emulate asynchronous behavior.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

rmad::RmadState CreateWelcomeState() {
  rmad::RmadState state;
  state.set_allocated_welcome(new rmad::WelcomeState());
  return state;
}

rmad::RmadState CreateSelectNetworkState() {
  rmad::RmadState state;
  state.set_allocated_select_network(new rmad::SelectNetworkState());
  return state;
}

rmad::GetStateReply CreateWelcomeStateReply(rmad::RmadErrorCode error) {
  rmad::GetStateReply reply;
  reply.set_allocated_state(new rmad::RmadState());
  reply.mutable_state()->set_allocated_welcome(new rmad::WelcomeState());
  reply.set_error(error);
  return reply;
}

rmad::GetStateReply CreateSelectNetworkStateReply(rmad::RmadErrorCode error) {
  rmad::GetStateReply reply;
  reply.set_allocated_state(new rmad::RmadState());
  reply.mutable_state()->set_allocated_select_network(
      new rmad::SelectNetworkState());
  reply.set_error(error);
  return reply;
}

TEST_F(FakeRmadClientTest, GetCurrentState_Default_RmaNotRequired) {
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

TEST_F(FakeRmadClientTest, GetCurrentState_Welcome_Ok) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;
  client_->GetCurrentState(base::BindLambdaForTesting(
      [&](absl::optional<rmad::GetStateReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
        EXPECT_TRUE(response->has_state());
        EXPECT_TRUE(response->state().has_welcome());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(FakeRmadClientTest, GetCurrentState_Welcome_CorrectStateReturned) {
  std::vector<rmad::GetStateReply> fake_states;
  // Use any error to test.
  rmad::GetStateReply state =
      CreateWelcomeStateReply(rmad::RMAD_ERROR_MISSING_COMPONENT);
  state.mutable_state()->mutable_welcome()->set_choice(
      rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_FINALIZE_REPAIR);
  fake_states.push_back(std::move(state));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;
  client_->GetCurrentState(base::BindLambdaForTesting(
      [&](absl::optional<rmad::GetStateReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_MISSING_COMPONENT);
        EXPECT_TRUE(response->has_state());
        EXPECT_TRUE(response->state().has_welcome());
        EXPECT_EQ(
            response->state().welcome().choice(),
            rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_FINALIZE_REPAIR);
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(FakeRmadClientTest, TransitionNextState_Default_RmaNotRequired) {
  base::RunLoop run_loop;
  client_->TransitionNextState(
      std::move(CreateWelcomeState()),
      base::BindLambdaForTesting(
          [&](absl::optional<rmad::GetStateReply> response) {
            EXPECT_TRUE(response.has_value());
            EXPECT_EQ(response->error(), rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
            EXPECT_FALSE(response->has_state());
            run_loop.Quit();
          }));
  run_loop.RunUntilIdle();
}

TEST_F(FakeRmadClientTest, TransitionNextState_NoNextState_Fails) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      rmad::GetStateReply(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;
  client_->TransitionNextState(
      std::move(CreateWelcomeState()),
      base::BindLambdaForTesting(
          [&](absl::optional<rmad::GetStateReply> response) {
            EXPECT_TRUE(response.has_value());
            EXPECT_EQ(response->error(), rmad::RMAD_ERROR_TRANSITION_FAILED);
            EXPECT_TRUE(response->has_state());
            EXPECT_TRUE(response->state().has_welcome());
            run_loop.Quit();
          }));
  run_loop.RunUntilIdle();
}

TEST_F(FakeRmadClientTest, TransitionNextState_HasNextState_Ok) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      rmad::GetStateReply(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK)));
  fake_states.push_back(
      rmad::GetStateReply(CreateSelectNetworkStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;
  client_->TransitionNextState(
      std::move(CreateWelcomeState()),
      base::BindLambdaForTesting(
          [&](absl::optional<rmad::GetStateReply> response) {
            EXPECT_TRUE(response.has_value());
            EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
            EXPECT_TRUE(response->has_state());
            EXPECT_TRUE(response->state().has_select_network());
            run_loop.Quit();
          }));
  run_loop.RunUntilIdle();
}

TEST_F(FakeRmadClientTest, TransitionNextState_WrongCurrentState_Invalid) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      rmad::GetStateReply(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK)));
  fake_states.push_back(
      rmad::GetStateReply(CreateSelectNetworkStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  base::RunLoop run_loop;
  client_->TransitionNextState(
      std::move(CreateSelectNetworkState()),
      base::BindLambdaForTesting(
          [&](absl::optional<rmad::GetStateReply> response) {
            EXPECT_TRUE(response.has_value());
            EXPECT_EQ(response->error(), rmad::RMAD_ERROR_REQUEST_INVALID);
            EXPECT_TRUE(response->has_state());
            EXPECT_TRUE(response->state().has_welcome());
            run_loop.Quit();
          }));
  run_loop.RunUntilIdle();
}

TEST_F(FakeRmadClientTest, TransitionPreviousState_Default_RmaNotRequired) {
  base::RunLoop run_loop;
  client_->TransitionPreviousState(base::BindLambdaForTesting(
      [&](absl::optional<rmad::GetStateReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
        EXPECT_FALSE(response->has_state());
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(FakeRmadClientTest, TransitionPreviousState_HasPreviousState_Ok) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(
      rmad::GetStateReply(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK)));
  fake_states.push_back(
      rmad::GetStateReply(CreateSelectNetworkStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  {
    base::RunLoop run_loop;
    client_->TransitionNextState(
        std::move(CreateWelcomeState()),
        base::BindLambdaForTesting(
            [&](absl::optional<rmad::GetStateReply> response) {
              EXPECT_TRUE(response.has_value());
              EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
              EXPECT_TRUE(response->has_state());
              EXPECT_TRUE(response->state().has_select_network());
              run_loop.Quit();
            }));
    run_loop.RunUntilIdle();
  }
  {
    base::RunLoop run_loop;
    client_->TransitionPreviousState(base::BindLambdaForTesting(
        [&](absl::optional<rmad::GetStateReply> response) {
          LOG(ERROR) << "Prev started";
          EXPECT_TRUE(response.has_value());
          EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
          EXPECT_TRUE(response->has_state());
          EXPECT_TRUE(response->state().has_welcome());
          run_loop.Quit();
        }));
    run_loop.RunUntilIdle();
  }
}

TEST_F(FakeRmadClientTest,
       TransitionPreviousState_HasPreviousState_StateUpdated) {
  std::vector<rmad::GetStateReply> fake_states;
  fake_states.push_back(CreateWelcomeStateReply(rmad::RMAD_ERROR_OK));
  fake_states.push_back(
      rmad::GetStateReply(CreateSelectNetworkStateReply(rmad::RMAD_ERROR_OK)));
  fake_client_()->SetFakeStateReplies(std::move(fake_states));

  {
    base::RunLoop run_loop;
    client_->GetCurrentState(base::BindLambdaForTesting(
        [&](absl::optional<rmad::GetStateReply> response) {
          EXPECT_TRUE(response.has_value());
          EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
          EXPECT_TRUE(response->has_state());
          EXPECT_TRUE(response->state().has_welcome());
          EXPECT_EQ(response->state().welcome().choice(),
                    rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_UNKNOWN);
          run_loop.Quit();
        }));
    run_loop.RunUntilIdle();
  }
  {
    rmad::RmadState current_state = CreateWelcomeState();
    current_state.mutable_welcome()->set_choice(
        rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_CANCEL);

    base::RunLoop run_loop;
    client_->TransitionNextState(
        std::move(current_state),
        base::BindLambdaForTesting(
            [&](absl::optional<rmad::GetStateReply> response) {
              EXPECT_TRUE(response.has_value());
              EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
              EXPECT_TRUE(response->has_state());
              EXPECT_TRUE(response->state().has_select_network());
              run_loop.Quit();
            }));
    run_loop.RunUntilIdle();
  }
  {
    base::RunLoop run_loop;
    client_->TransitionPreviousState(base::BindLambdaForTesting(
        [&](absl::optional<rmad::GetStateReply> response) {
          LOG(ERROR) << "Prev started";
          EXPECT_TRUE(response.has_value());
          EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
          EXPECT_TRUE(response->has_state());
          EXPECT_TRUE(response->state().has_welcome());
          EXPECT_EQ(response->state().welcome().choice(),
                    rmad::WelcomeState_FinalizeChoice_RMAD_CHOICE_CANCEL);
          run_loop.Quit();
        }));
    run_loop.RunUntilIdle();
  }
}

TEST_F(FakeRmadClientTest, Abortable_Default_Ok) {
  base::RunLoop run_loop;
  client_->AbortRma(base::BindLambdaForTesting(
      [&](absl::optional<rmad::AbortRmaReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(FakeRmadClientTest, Abortable_SetFalse_CannotCancel) {
  fake_client_()->SetAbortable(false);
  base::RunLoop run_loop;
  client_->AbortRma(base::BindLambdaForTesting(
      [&](absl::optional<rmad::AbortRmaReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_CANNOT_CANCEL_RMA);
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

TEST_F(FakeRmadClientTest, Abortable_SetTrue_Ok) {
  fake_client_()->SetAbortable(true);
  base::RunLoop run_loop;
  client_->AbortRma(base::BindLambdaForTesting(
      [&](absl::optional<rmad::AbortRmaReply> response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->error(), rmad::RMAD_ERROR_OK);
        run_loop.Quit();
      }));
  run_loop.RunUntilIdle();
}

}  // namespace
}  // namespace chromeos
