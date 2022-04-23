// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_manager.h"

#include <memory>
#include <vector>

#include "base/barrier_callback.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/callback_command.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using ::testing::StrictMock;

class MockCommand : public WebAppCommand {
 public:
  explicit MockCommand(WebAppCommandLock command_lock)
      : WebAppCommand(std::move(command_lock)) {}

  MOCK_METHOD(void, OnDestruction, ());

  ~MockCommand() override { OnDestruction(); }

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, OnBeforeForcedUninstallFromSync, (), (override));
  MOCK_METHOD(void, OnShutdown, (), (override));

  base::WeakPtr<MockCommand> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  base::Value ToDebugValue() const override {
    return base::Value("FakeCommand");
  }

  void CallSignalCompletionAndSelfDestruct(
      CommandResult result,
      base::OnceClosure completion_callback) {
    WebAppCommand::SignalCompletionAndSelfDestruct(
        result, std::move(completion_callback));
  }

 private:
  base::WeakPtrFactory<MockCommand> weak_factory_{this};
};

class WebAppCommandManagerTest : public ::testing::Test {
 public:
  static const constexpr char kTestAppId[] = "test_app_id";
  static const constexpr char kTestAppId2[] = "test_app_id_2";

  WebAppCommandManagerTest() = default;
  ~WebAppCommandManagerTest() override = default;

  void CheckCommandsRunInOrder(base::WeakPtr<MockCommand> command1_ptr,
                               base::WeakPtr<MockCommand> command2_ptr) {
    ASSERT_TRUE(command1_ptr && command2_ptr);
    EXPECT_FALSE(command1_ptr->IsStarted() || command2_ptr->IsStarted());

    testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
    {
      base::RunLoop loop;
      testing::InSequence in_sequence;
      EXPECT_CALL(*command1_ptr, Start()).Times(1).WillOnce([&]() {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              command1_ptr->CallSignalCompletionAndSelfDestruct(
                  CommandResult::kSuccess, mock_closure.Get());
            }));
      });

      EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);
      EXPECT_CALL(mock_closure, Run()).Times(1);

      EXPECT_CALL(*command2_ptr, Start()).Times(1).WillOnce([&]() {
        EXPECT_FALSE(command1_ptr);
        command2_ptr->CallSignalCompletionAndSelfDestruct(
            CommandResult::kSuccess, mock_closure.Get());
      });
      EXPECT_CALL(*command2_ptr, OnDestruction()).Times(1);
      EXPECT_CALL(mock_closure, Run()).Times(1).WillOnce([&]() {
        loop.Quit();
      });
      loop.Run();
    }
    EXPECT_FALSE(command1_ptr);
    EXPECT_FALSE(command2_ptr);
  }

  void CheckCommandsRunInParallel(base::WeakPtr<MockCommand> command1_ptr,
                                  base::WeakPtr<MockCommand> command2_ptr) {
    testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
    ASSERT_TRUE(command1_ptr && command2_ptr);
    EXPECT_FALSE(command1_ptr->IsStarted() || command2_ptr->IsStarted());

    {
      base::RunLoop loop;
      testing::InSequence in_sequence;

      EXPECT_CALL(*command1_ptr, Start()).Times(1);

      // Only signal completion of command1 after command2 is started to test
      // that starting of command2 is not blocked by command1.
      EXPECT_CALL(*command2_ptr, Start()).WillOnce([&]() {
        command2_ptr->CallSignalCompletionAndSelfDestruct(
            CommandResult::kSuccess, mock_closure.Get());
        command1_ptr->CallSignalCompletionAndSelfDestruct(
            CommandResult::kSuccess, mock_closure.Get());
      });
      EXPECT_CALL(*command2_ptr, OnDestruction()).Times(1);
      EXPECT_CALL(mock_closure, Run()).Times(1);

      EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);
      EXPECT_CALL(mock_closure, Run()).Times(1).WillOnce([&]() {
        loop.Quit();
      });
      loop.Run();
    }
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebAppCommandManagerTest, SimpleCommand) {
  WebAppCommandManager manager;

  // Simple test of a command enqueued, starting, and completing.
  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
  auto mock_command = std::make_unique<::testing::StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  base::WeakPtr<MockCommand> command_ptr = mock_command->AsWeakPtr();

  manager.EnqueueCommand(std::move(mock_command));
  ASSERT_TRUE(command_ptr);
  EXPECT_FALSE(command_ptr->IsStarted());
  {
    base::RunLoop loop;
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, Start()).WillOnce([&]() { loop.Quit(); });
    loop.Run();
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(mock_closure, Run()).Times(1);
    command_ptr->CallSignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                                     mock_closure.Get());
  }
  EXPECT_FALSE(command_ptr);
  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, CompleteInStart) {
  WebAppCommandManager manager;

  // Test to make sure the command can complete & destroy itself in the Start
  // method.

  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
  auto command = std::make_unique<::testing::StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  base::WeakPtr<MockCommand> command_ptr = command->AsWeakPtr();

  manager.EnqueueCommand(std::move(command));
  {
    base::RunLoop loop;
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, Start()).Times(1).WillOnce([&]() {
      ASSERT_TRUE(command_ptr);
      command_ptr->CallSignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                                       mock_closure.Get());
    });
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(mock_closure, Run()).Times(1).WillOnce([&]() { loop.Quit(); });
    loop.Run();
  }
  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, TwoQueues) {
  WebAppCommandManager manager;

  auto command1 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForAppLock({kTestAppId}));
  auto command2 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForAppLock({kTestAppId2}));
  base::WeakPtr<MockCommand> command1_ptr = command1->AsWeakPtr();
  base::WeakPtr<MockCommand> command2_ptr = command2->AsWeakPtr();

  manager.EnqueueCommand(std::move(command1));
  manager.EnqueueCommand(std::move(command2));
  CheckCommandsRunInParallel(command1_ptr, command2_ptr);

  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, MixedQueueTypes) {
  WebAppCommandManager manager;

  auto command1 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  auto command2 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForAppLock({kTestAppId}));
  base::WeakPtr<MockCommand> command1_ptr = command1->AsWeakPtr();
  base::WeakPtr<MockCommand> command2_ptr = command2->AsWeakPtr();

  manager.EnqueueCommand(std::move(command1));
  manager.EnqueueCommand(std::move(command2));
  // Global command blocks app command.
  CheckCommandsRunInOrder(command1_ptr, command2_ptr);

  command1 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  command2 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForBackgroundWebContentsLock());
  command1_ptr = command1->AsWeakPtr();
  command2_ptr = command2->AsWeakPtr();

  manager.EnqueueCommand(std::move(command1));
  manager.EnqueueCommand(std::move(command2));
  // Global command blocks web contents command.
  CheckCommandsRunInOrder(command1_ptr, command2_ptr);

  command1 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForAppLock({kTestAppId}));
  command2 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForBackgroundWebContentsLock());
  command1_ptr = command1->AsWeakPtr();
  command2_ptr = command2->AsWeakPtr();

  manager.EnqueueCommand(std::move(command1));
  manager.EnqueueCommand(std::move(command2));
  // App command and web contents command queue are independent.
  CheckCommandsRunInParallel(command1_ptr, command2_ptr);

  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, SingleAppQueue) {
  WebAppCommandManager manager;

  auto command1 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForAppLock({kTestAppId}));
  base::WeakPtr<MockCommand> command1_ptr = command1->AsWeakPtr();

  auto command2 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForAppLock({kTestAppId}));
  base::WeakPtr<MockCommand> command2_ptr = command2->AsWeakPtr();

  manager.EnqueueCommand(std::move(command1));
  manager.EnqueueCommand(std::move(command2));
  CheckCommandsRunInOrder(command1_ptr, command2_ptr);

  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, GlobalQueue) {
  WebAppCommandManager manager;

  auto command1 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  base::WeakPtr<MockCommand> command1_ptr = command1->AsWeakPtr();

  auto command2 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  base::WeakPtr<MockCommand> command2_ptr = command2->AsWeakPtr();

  manager.EnqueueCommand(std::move(command1));
  manager.EnqueueCommand(std::move(command2));
  CheckCommandsRunInOrder(command1_ptr, command2_ptr);

  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, BackgroundWebContentsQueue) {
  WebAppCommandManager manager;

  auto command1 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForBackgroundWebContentsLock());
  base::WeakPtr<MockCommand> command1_ptr = command1->AsWeakPtr();

  auto command2 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForBackgroundWebContentsLock());
  base::WeakPtr<MockCommand> command2_ptr = command2->AsWeakPtr();

  manager.EnqueueCommand(std::move(command1));
  manager.EnqueueCommand(std::move(command2));
  CheckCommandsRunInOrder(command1_ptr, command2_ptr);

  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, ShutdownPreStartCommand) {
  WebAppCommandManager manager;

  auto command = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  base::WeakPtr<MockCommand> command_ptr = command->AsWeakPtr();
  manager.EnqueueCommand(std::move(command));
  EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, ShutdownStartedCommand) {
  WebAppCommandManager manager;

  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
  auto mock_command = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  base::WeakPtr<MockCommand> command_ptr = mock_command->AsWeakPtr();

  manager.EnqueueCommand(std::move(mock_command));
  ASSERT_TRUE(command_ptr);
  EXPECT_FALSE(command_ptr->IsStarted());
  {
    base::RunLoop loop;
    EXPECT_CALL(*command_ptr, Start()).WillOnce([&]() { loop.Quit(); });
    loop.Run();
  }
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, OnShutdown()).Times(1);
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
  }
  manager.Shutdown();
  EXPECT_FALSE(command_ptr);
}

TEST_F(WebAppCommandManagerTest, ShutdownQueuedCommand) {
  WebAppCommandManager manager;

  auto command1 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  base::WeakPtr<MockCommand> command1_ptr = command1->AsWeakPtr();

  auto command2 = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  base::WeakPtr<MockCommand> command2_ptr = command2->AsWeakPtr();

  manager.EnqueueCommand(std::move(command1));
  manager.EnqueueCommand(std::move(command2));
  {
    base::RunLoop loop;
    EXPECT_CALL(*command1_ptr, Start()).WillOnce([&]() { loop.Quit(); });
    loop.Run();
  }
  EXPECT_CALL(*command1_ptr, OnShutdown()).Times(1);
  EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);
  EXPECT_CALL(*command2_ptr, OnDestruction()).Times(1);
  manager.Shutdown();
  EXPECT_FALSE(command1_ptr || command2_ptr);
}

TEST_F(WebAppCommandManagerTest, OnShutdownCallsCompleteAndDestruct) {
  WebAppCommandManager manager;

  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
  auto command = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForFullSystemLock());
  base::WeakPtr<MockCommand> command_ptr = command->AsWeakPtr();
  manager.EnqueueCommand(std::move(command));
  {
    base::RunLoop loop;
    EXPECT_CALL(*command_ptr, Start()).WillOnce([&]() { loop.Quit(); });
    loop.Run();
  }
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, OnShutdown()).Times(1).WillOnce([&]() {
      ASSERT_TRUE(command_ptr);
      command_ptr->CallSignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                                       mock_closure.Get());
    });
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(mock_closure, Run()).Times(1);
  }
  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, NotifySyncCallsCompleteAndDestruct) {
  WebAppCommandManager manager;

  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
  auto command = std::make_unique<StrictMock<MockCommand>>(
      WebAppCommandLock::CreateForAppLock({kTestAppId}));
  base::WeakPtr<MockCommand> command_ptr = command->AsWeakPtr();
  manager.EnqueueCommand(std::move(command));
  {
    base::RunLoop loop;
    EXPECT_CALL(*command_ptr, Start()).WillOnce([&]() { loop.Quit(); });
    loop.Run();
  }
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, OnBeforeForcedUninstallFromSync())
        .Times(1)
        .WillOnce([&]() {
          ASSERT_TRUE(command_ptr);
          command_ptr->CallSignalCompletionAndSelfDestruct(
              CommandResult::kSuccess, mock_closure.Get());
        });
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(mock_closure, Run()).Times(1);
  }
  manager.NotifyBeforeSyncUninstalls({kTestAppId});
  manager.Shutdown();
}

TEST_F(WebAppCommandManagerTest, MultipleCallbackCommands) {
  WebAppCommandManager manager;
  base::RunLoop loop;
  // Queue multiple callbacks to app queues, and gather output.
  auto barrier = base::BarrierCallback<std::string>(
      2, base::BindLambdaForTesting([&](std::vector<std::string> result) {
        EXPECT_EQ(result.size(), 2u);
        loop.Quit();
      }));
  for (auto* app_id : {kTestAppId, kTestAppId2}) {
    base::OnceClosure callback = base::BindOnce(
        [](AppId app_id, base::RepeatingCallback<void(std::string)> barrier) {
          barrier.Run(app_id);
        },
        app_id, barrier);
    manager.EnqueueCommand(std::make_unique<CallbackCommand>(
        WebAppCommandLock::CreateForAppLock({app_id}), std::move(callback)));
  }
  loop.Run();
  manager.Shutdown();
}

}  // namespace
}  // namespace web_app
