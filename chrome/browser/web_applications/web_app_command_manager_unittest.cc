// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_manager.h"

#include <memory>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/callback_command.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}

namespace web_app {
namespace {

using ::testing::StrictMock;

template <typename LockType>
class MockCommand : public WebAppCommandTemplate<LockType> {
 public:
  explicit MockCommand(
      std::unique_ptr<typename LockType::LockDescription> lock_description)
      : WebAppCommandTemplate<LockType>("MockCommand"),
        lock_description_(std::move(lock_description)) {}

  MOCK_METHOD(void, OnDestruction, ());

  ~MockCommand() override { OnDestruction(); }

  const LockDescription& lock_description() const override {
    return *lock_description_;
  }

  MOCK_METHOD(void, StartWithLock, (std::unique_ptr<LockType>), (override));
  MOCK_METHOD(void, OnShutdown, (), (override));

  base::WeakPtr<MockCommand> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  base::Value ToDebugValue() const override {
    return base::Value("FakeCommand");
  }

  void PostSignalCompletionAndSelfDestruct(
      CommandResult result,
      base::OnceClosure completion_callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MockCommand::CallSignalCompletionAndSelfDestruct,
                       weak_factory_.GetWeakPtr(), result,
                       std::move(completion_callback)));
  }

  void CallSignalCompletionAndSelfDestruct(
      CommandResult result,
      base::OnceClosure completion_callback) {
    WebAppCommand::SignalCompletionAndSelfDestruct(
        result, std::move(completion_callback));
  }

 private:
  std::unique_ptr<LockDescription> lock_description_;

  base::WeakPtrFactory<MockCommand> weak_factory_{this};
};

class WebAppCommandManagerTest : public WebAppTest {
 public:
  static const constexpr char kTestAppId[] = "test_app_id_1";
  static const constexpr char kTestAppId2[] = "test_app_id_2";

  WebAppCommandManagerTest() = default;
  ~WebAppCommandManagerTest() override = default;

  WebAppCommandManager& manager() {
    return WebAppProvider::GetForTest(profile())->command_manager();
  }

  void SetUp() override {
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->StartWithSubsystems();
  }

  void TearDown() override {
    // TestingProfile checks for leaked RenderWidgetHosts before shutting down
    // the profile, so we must shutdown first to delete the shared web contents
    // before tearing down.
    manager().Shutdown();
    WebAppTest::TearDown();
  }

  template <typename LockType1, typename LockType2>
  void CheckCommandsRunInOrder(
      base::WeakPtr<MockCommand<LockType1>> command1_ptr,
      base::WeakPtr<MockCommand<LockType2>> command2_ptr) {
    ASSERT_TRUE(command1_ptr && command2_ptr);
    EXPECT_FALSE(command1_ptr->IsStarted() || command2_ptr->IsStarted());

    testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
    {
      base::test::TestFuture<void> first_command_done;
      testing::InSequence in_sequence;
      EXPECT_CALL(*command1_ptr, StartWithLock(testing::_))
          .Times(1)
          .WillOnce([&]() {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindLambdaForTesting([&]() {
                  // Post this to catch if the second command runs before this
                  // one completes.
                  command1_ptr->PostSignalCompletionAndSelfDestruct(
                      CommandResult::kSuccess,
                      first_command_done.GetCallback());
                }));
          });

      EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);
      // Wait until the first command is done to verify that the second command
      // isn't run before this one completes.
      ASSERT_TRUE(first_command_done.Wait());

      base::RunLoop loop;
      EXPECT_CALL(*command2_ptr, StartWithLock(testing::_))
          .Times(1)
          .WillOnce([&]() {
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

  template <typename LockType1, typename LockType2>
  void CheckCommandsRunInParallel(
      base::WeakPtr<MockCommand<LockType1>> command1_ptr,
      base::WeakPtr<MockCommand<LockType2>> command2_ptr) {
    testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
    ASSERT_TRUE(command1_ptr && command2_ptr);
    EXPECT_FALSE(command1_ptr->IsStarted() || command2_ptr->IsStarted());

    {
      base::RunLoop loop;
      testing::InSequence in_sequence;

      EXPECT_CALL(*command1_ptr, StartWithLock(testing::_)).Times(1);

      // Only signal completion of command1 after command2 is started to test
      // that starting of command2 is not blocked by command1.
      EXPECT_CALL(*command2_ptr, StartWithLock(testing::_)).WillOnce([&]() {
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
};

TEST_F(WebAppCommandManagerTest, SimpleCommand) {
  // Simple test of a command enqueued, starting, and completing.
  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
  auto mock_command =
      std::make_unique<::testing::StrictMock<MockCommand<AllAppsLock>>>(
          std::make_unique<AllAppsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr =
      mock_command->AsWeakPtr();

  manager().ScheduleCommand(std::move(mock_command));
  ASSERT_TRUE(command_ptr);
  EXPECT_FALSE(command_ptr->IsStarted());
  {
    base::RunLoop loop;
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, StartWithLock(testing::_)).WillOnce([&]() {
      loop.Quit();
    });
    loop.Run();
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(mock_closure, Run()).Times(1);
    command_ptr->CallSignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                                     mock_closure.Get());
  }
  EXPECT_FALSE(command_ptr);
}

TEST_F(WebAppCommandManagerTest, CompleteInStart) {
  // Test to make sure the command can complete & destroy itself in the Start
  // method.
  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
  auto command =
      std::make_unique<::testing::StrictMock<MockCommand<AllAppsLock>>>(
          std::make_unique<AllAppsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr = command->AsWeakPtr();

  manager().ScheduleCommand(std::move(command));
  {
    base::RunLoop loop;
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, StartWithLock(testing::_))
        .Times(1)
        .WillOnce([&]() {
          ASSERT_TRUE(command_ptr);
          command_ptr->CallSignalCompletionAndSelfDestruct(
              CommandResult::kSuccess, mock_closure.Get());
        });
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(mock_closure, Run()).Times(1).WillOnce([&]() { loop.Quit(); });
    loop.Run();
  }
}

TEST_F(WebAppCommandManagerTest, TwoQueues) {
  auto command1 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      std::make_unique<AppLockDescription>(kTestAppId));
  auto command2 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      std::make_unique<AppLockDescription>(kTestAppId2));
  base::WeakPtr<MockCommand<AppLock>> command1_ptr = command1->AsWeakPtr();
  base::WeakPtr<MockCommand<AppLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  CheckCommandsRunInParallel(command1_ptr, command2_ptr);
}

TEST_F(WebAppCommandManagerTest, MixedQueueTypes) {
  auto command1 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      std::make_unique<AllAppsLockDescription>());
  auto command2 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      std::make_unique<AppLockDescription>(kTestAppId));
  base::WeakPtr<MockCommand<AllAppsLock>> command1_ptr = command1->AsWeakPtr();
  base::WeakPtr<MockCommand<AppLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  // Global command blocks app command.
  CheckCommandsRunInOrder(command1_ptr, command2_ptr);

  auto command3 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      std::make_unique<AllAppsLockDescription>());
  auto command4 =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          std::make_unique<SharedWebContentsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command3_ptr = command3->AsWeakPtr();
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command4_ptr =
      command4->AsWeakPtr();

  // One about:blank load per web contents lock.
  manager().ScheduleCommand(std::move(command3));
  manager().ScheduleCommand(std::move(command4));
  // All app lock does not block web contents command.
  CheckCommandsRunInParallel(command3_ptr, command4_ptr);

  auto command5 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      std::make_unique<AppLockDescription>(kTestAppId));
  auto command6 =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          std::make_unique<SharedWebContentsLockDescription>());
  base::WeakPtr<MockCommand<AppLock>> command5_ptr = command5->AsWeakPtr();
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command6_ptr =
      command6->AsWeakPtr();

  manager().ScheduleCommand(std::move(command5));
  manager().ScheduleCommand(std::move(command6));
  // App command and web contents command queue are independent.
  CheckCommandsRunInParallel(command5_ptr, command6_ptr);
}

TEST_F(WebAppCommandManagerTest, SingleAppQueue) {
  auto command1 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      std::make_unique<AppLockDescription>(kTestAppId));
  base::WeakPtr<MockCommand<AppLock>> command1_ptr = command1->AsWeakPtr();

  auto command2 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      std::make_unique<AppLockDescription>(kTestAppId));
  base::WeakPtr<MockCommand<AppLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  CheckCommandsRunInOrder(command1_ptr, command2_ptr);
}

TEST_F(WebAppCommandManagerTest, GlobalQueue) {
  auto command1 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      std::make_unique<AllAppsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command1_ptr = command1->AsWeakPtr();

  auto command2 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      std::make_unique<AllAppsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  CheckCommandsRunInOrder(command1_ptr, command2_ptr);
}

TEST_F(WebAppCommandManagerTest, BackgroundWebContentsQueue) {
  auto command1 =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          std::make_unique<SharedWebContentsLockDescription>());
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command1_ptr =
      command1->AsWeakPtr();

  auto command2 =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          std::make_unique<SharedWebContentsLockDescription>());
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command2_ptr =
      command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  CheckCommandsRunInOrder(command1_ptr, command2_ptr);
}

TEST_F(WebAppCommandManagerTest, ShutdownPreStartCommand) {
  auto command = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      std::make_unique<AllAppsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr = command->AsWeakPtr();
  manager().ScheduleCommand(std::move(command));
  EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
  manager().Shutdown();
}

TEST_F(WebAppCommandManagerTest, ShutdownStartedCommand) {
  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
  auto mock_command = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      std::make_unique<AllAppsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr =
      mock_command->AsWeakPtr();

  manager().ScheduleCommand(std::move(mock_command));
  ASSERT_TRUE(command_ptr);
  EXPECT_FALSE(command_ptr->IsStarted());
  {
    base::RunLoop loop;
    EXPECT_CALL(*command_ptr, StartWithLock(testing::_)).WillOnce([&]() {
      loop.Quit();
    });
    loop.Run();
  }
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, OnShutdown()).Times(1);
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
  }
  manager().Shutdown();
  EXPECT_FALSE(command_ptr);
}

TEST_F(WebAppCommandManagerTest, ShutdownQueuedCommand) {
  auto command1 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      std::make_unique<AllAppsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command1_ptr = command1->AsWeakPtr();

  auto command2 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      std::make_unique<AllAppsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  {
    base::RunLoop loop;
    EXPECT_CALL(*command1_ptr, StartWithLock(testing::_)).WillOnce([&]() {
      loop.Quit();
    });
    loop.Run();
  }
  EXPECT_CALL(*command1_ptr, OnShutdown()).Times(1);
  EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);
  EXPECT_CALL(*command2_ptr, OnDestruction()).Times(1);
}

TEST_F(WebAppCommandManagerTest, OnShutdownCallsCompleteAndDestruct) {
  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;
  auto command = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      std::make_unique<AllAppsLockDescription>());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr = command->AsWeakPtr();
  manager().ScheduleCommand(std::move(command));
  {
    base::RunLoop loop;
    EXPECT_CALL(*command_ptr, StartWithLock(testing::_)).WillOnce([&]() {
      loop.Quit();
    });
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
  manager().Shutdown();
}

TEST_F(WebAppCommandManagerTest, MultipleCallbackCommands) {
  base::RunLoop loop;
  // Queue multiple callbacks to app queues, and gather output.
  auto barrier = base::BarrierCallback<std::string>(
      2, base::BindLambdaForTesting([&](std::vector<std::string> result) {
        EXPECT_EQ(result.size(), 2u);
        loop.Quit();
      }));
  for (auto* app_id : {kTestAppId, kTestAppId2}) {
    base::OnceCallback<void(AppLock&)> callback =
        base::BindOnce([](webapps::AppId app_id,
                          base::RepeatingCallback<void(std::string)> barrier,
                          AppLock&) { barrier.Run(app_id); },
                       app_id, barrier);
    manager().ScheduleCommand(std::make_unique<CallbackCommand<AppLock>>(
        "", std::make_unique<AppLockDescription>(app_id), std::move(callback)));
  }
  loop.Run();
}

TEST_F(WebAppCommandManagerTest, AppWithSharedWebContents) {
  auto command1 = std::make_unique<MockCommand<SharedWebContentsWithAppLock>>(
      std::make_unique<SharedWebContentsWithAppLockDescription,
                       base::flat_set<webapps::AppId>>({kTestAppId}));
  auto command2 = std::make_unique<MockCommand<AppLock>>(
      std::make_unique<AppLockDescription>(kTestAppId));
  auto command3 = std::make_unique<MockCommand<SharedWebContentsLock>>(
      std::make_unique<SharedWebContentsLockDescription>());
  base::WeakPtr<MockCommand<SharedWebContentsWithAppLock>> command1_ptr =
      command1->AsWeakPtr();
  base::WeakPtr<MockCommand<AppLock>> command2_ptr = command2->AsWeakPtr();
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command3_ptr =
      command3->AsWeakPtr();

  testing::StrictMock<base::MockCallback<base::OnceClosure>> mock_closure;

  // One about:blank load per web contents lock.
  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  manager().ScheduleCommand(std::move(command3));
  {
    base::RunLoop loop;
    testing::InSequence in_sequence;
    EXPECT_CALL(*command1_ptr, StartWithLock(testing::_))
        .Times(1)
        .WillOnce([&]() {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindLambdaForTesting([&]() {
                command1_ptr->CallSignalCompletionAndSelfDestruct(
                    CommandResult::kSuccess, mock_closure.Get());
              }));
        });
    EXPECT_CALL(*command2_ptr, StartWithLock(testing::_)).Times(0);
    EXPECT_CALL(*command3_ptr, StartWithLock(testing::_)).Times(0);
    EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(mock_closure, Run())
        .WillOnce(base::test::RunClosure(loop.QuitClosure()));
    loop.Run();
  }
  EXPECT_FALSE(command1_ptr);
  {
    EXPECT_CALL(*command2_ptr, StartWithLock(testing::_))
        .Times(1)
        .WillOnce([&]() {
          command2_ptr->CallSignalCompletionAndSelfDestruct(
              CommandResult::kSuccess, mock_closure.Get());
        });
    EXPECT_CALL(*command3_ptr, StartWithLock(testing::_))
        .Times(1)
        .WillOnce([&]() {
          command3_ptr->CallSignalCompletionAndSelfDestruct(
              CommandResult::kSuccess, mock_closure.Get());
        });
    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, loop.QuitClosure());
    EXPECT_CALL(*command2_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(*command3_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(mock_closure, Run())
        .Times(2)
        .WillRepeatedly(base::test::RunClosure(barrier));
    loop.Run();
  }
  EXPECT_FALSE(command1_ptr);
  EXPECT_FALSE(command2_ptr);
  EXPECT_FALSE(command3_ptr);
}

TEST_F(WebAppCommandManagerTest, ToDebugValue) {
  base::RunLoop loop;
  manager().ScheduleCommand(std::make_unique<CallbackCommand<AppLock>>(
      "", std::make_unique<AppLockDescription>(kTestAppId),
      base::BindLambdaForTesting([&](AppLock&) { loop.Quit(); })));
  manager().ScheduleCommand(std::make_unique<CallbackCommand<AppLock>>(
      "", std::make_unique<AppLockDescription>(kTestAppId2),
      base::DoNothingAs<void(AppLock&)>()));
  loop.Run();
  manager().ToDebugValue();
}

}  // namespace
}  // namespace web_app
