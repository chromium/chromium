// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_manager.h"

#include <iterator>
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
#include "chrome/browser/web_applications/commands/internal/callback_command.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_observer_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}

namespace web_app {
namespace {

using ::testing::StrictMock;

template <typename LockType>
class MockCommand : public WebAppCommand<LockType> {
 public:
  explicit MockCommand(LockType::LockDescription lock_description,
                       base::OnceClosure on_complete)
      : WebAppCommand<LockType>("MockCommand",
                                std::move(lock_description),
                                std::move(on_complete)) {}

  MOCK_METHOD(void, OnDestruction, ());

  ~MockCommand() override { OnDestruction(); }

  void StartWithLock(std::unique_ptr<LockType> lock) override {
    lock_ = std::move(lock);
    StartAfterStoringLock();
  }

  MOCK_METHOD(void, StartAfterStoringLock, ());

  base::WeakPtr<MockCommand> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  void PostCompleteAndSelfDestruct(CommandResult result) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&MockCommand::CallCompleteAndSelfDestruct,
                                  weak_factory_.GetWeakPtr(), result));
  }

  void CallCompleteAndSelfDestruct(CommandResult result) {
    WebAppCommand<LockType>::CompleteAndSelfDestruct(result);
  }

 private:
  std::unique_ptr<LockType> lock_;
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
      base::test::TestFuture<void>& command1_done,
      base::WeakPtr<MockCommand<LockType2>> command2_ptr,
      base::test::TestFuture<void>& command2_done) {
    ASSERT_TRUE(command1_ptr && command2_ptr);
    EXPECT_FALSE(command1_ptr->IsStarted() || command2_ptr->IsStarted());

    {
      testing::InSequence in_sequence;
      EXPECT_CALL(*command1_ptr, StartAfterStoringLock())
          .Times(1)
          .WillOnce([&]() {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindLambdaForTesting([&]() {
                  // Post this to catch if the second command runs before this
                  // one completes.
                  command1_ptr->PostCompleteAndSelfDestruct(
                      CommandResult::kSuccess);
                }));
          });

      EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);

      EXPECT_CALL(*command2_ptr, StartAfterStoringLock())
          .Times(1)
          .WillOnce([&]() {
            EXPECT_FALSE(command1_ptr);
            command2_ptr->CallCompleteAndSelfDestruct(CommandResult::kSuccess);
          });
      EXPECT_CALL(*command2_ptr, OnDestruction()).Times(1);
      ASSERT_TRUE(command1_done.Wait());
      ASSERT_TRUE(command2_done.Wait());
    }
    EXPECT_FALSE(command1_ptr);
    EXPECT_FALSE(command2_ptr);
  }

  template <typename LockType1, typename LockType2>
  void CheckCommandsRunInParallel(
      base::WeakPtr<MockCommand<LockType1>> command1_ptr,
      base::test::TestFuture<void>& command1_done,
      base::WeakPtr<MockCommand<LockType2>> command2_ptr,
      base::test::TestFuture<void>& command2_done) {
    ASSERT_TRUE(command1_ptr && command2_ptr);
    EXPECT_FALSE(command1_ptr->IsStarted() || command2_ptr->IsStarted());

    {
      base::RunLoop loop;
      testing::InSequence in_sequence;

      EXPECT_CALL(*command1_ptr, StartAfterStoringLock()).Times(1);

      // Only signal completion of command1 after command2 is started to test
      // that starting of command2 is not blocked by command1.
      EXPECT_CALL(*command2_ptr, StartAfterStoringLock()).WillOnce([&]() {
        command2_ptr->CallCompleteAndSelfDestruct(CommandResult::kSuccess);
        command1_ptr->CallCompleteAndSelfDestruct(CommandResult::kSuccess);
      });
      EXPECT_CALL(*command2_ptr, OnDestruction()).Times(1);
      EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);
      ASSERT_TRUE(command2_done.Wait());
      ASSERT_TRUE(command1_done.Wait());
    }
  }
};

TEST_F(WebAppCommandManagerTest, SimpleCommand) {
  // Simple test of a command enqueued, starting, and completing.
  base::test::TestFuture<void> command_complete;
  auto mock_command =
      std::make_unique<::testing::StrictMock<MockCommand<AllAppsLock>>>(
          AllAppsLockDescription(), command_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr =
      mock_command->AsWeakPtr();

  manager().ScheduleCommand(std::move(mock_command));
  ASSERT_TRUE(command_ptr);
  EXPECT_FALSE(command_ptr->IsStarted());
  {
    base::RunLoop loop;
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, StartAfterStoringLock()).WillOnce([&]() {
      loop.Quit();
    });
    loop.Run();
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    command_ptr->CallCompleteAndSelfDestruct(CommandResult::kSuccess);
  }
  EXPECT_TRUE(command_complete.Wait());
  EXPECT_FALSE(command_ptr);
}

TEST_F(WebAppCommandManagerTest, CompleteInStart) {
  // Test to make sure the command can complete & destroy itself in the Start
  // method.
  base::test::TestFuture<void> command_complete;
  auto command =
      std::make_unique<::testing::StrictMock<MockCommand<AllAppsLock>>>(
          AllAppsLockDescription(), command_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr = command->AsWeakPtr();

  manager().ScheduleCommand(std::move(command));
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*command_ptr, StartAfterStoringLock()).Times(1).WillOnce([&]() {
      ASSERT_TRUE(command_ptr);
      command_ptr->CallCompleteAndSelfDestruct(CommandResult::kSuccess);
    });
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    EXPECT_TRUE(command_complete.Wait());
  }
}

TEST_F(WebAppCommandManagerTest, TwoQueues) {
  base::test::TestFuture<void> command1_complete;
  auto command1 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      AppLockDescription(kTestAppId), command1_complete.GetCallback());
  base::test::TestFuture<void> command2_complete;
  auto command2 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      AppLockDescription(kTestAppId2), command2_complete.GetCallback());
  base::WeakPtr<MockCommand<AppLock>> command1_ptr = command1->AsWeakPtr();
  base::WeakPtr<MockCommand<AppLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  CheckCommandsRunInParallel(command1_ptr, command1_complete, command2_ptr,
                             command2_complete);
}

TEST_F(WebAppCommandManagerTest, MixedQueueTypes) {
  base::test::TestFuture<void> command1_complete;
  auto command1 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      AllAppsLockDescription(), command1_complete.GetCallback());
  base::test::TestFuture<void> command2_complete;
  auto command2 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      AppLockDescription(kTestAppId), command2_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command1_ptr = command1->AsWeakPtr();
  base::WeakPtr<MockCommand<AppLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  // Global command blocks app command.
  CheckCommandsRunInOrder(command1_ptr, command1_complete, command2_ptr,
                          command2_complete);

  base::test::TestFuture<void> command3_complete;
  auto command3 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      AllAppsLockDescription(), command3_complete.GetCallback());
  base::test::TestFuture<void> command4_complete;
  auto command4 =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          SharedWebContentsLockDescription(), command4_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command3_ptr = command3->AsWeakPtr();
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command4_ptr =
      command4->AsWeakPtr();

  // One about:blank load per web contents lock.
  manager().ScheduleCommand(std::move(command3));
  manager().ScheduleCommand(std::move(command4));
  // All app lock does not block web contents command.
  CheckCommandsRunInParallel(command3_ptr, command3_complete, command4_ptr,
                             command4_complete);

  base::test::TestFuture<void> command5_complete;
  auto command5 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      AppLockDescription(kTestAppId), command5_complete.GetCallback());
  base::test::TestFuture<void> command6_complete;
  auto command6 =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          SharedWebContentsLockDescription(), command6_complete.GetCallback());
  base::WeakPtr<MockCommand<AppLock>> command5_ptr = command5->AsWeakPtr();
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command6_ptr =
      command6->AsWeakPtr();

  manager().ScheduleCommand(std::move(command5));
  manager().ScheduleCommand(std::move(command6));
  // App command and web contents command queue are independent.
  CheckCommandsRunInParallel(command5_ptr, command5_complete, command6_ptr,
                             command6_complete);
}

TEST_F(WebAppCommandManagerTest, SingleAppQueue) {
  base::test::TestFuture<void> command1_complete;
  auto command1 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      AppLockDescription(kTestAppId), command1_complete.GetCallback());
  base::WeakPtr<MockCommand<AppLock>> command1_ptr = command1->AsWeakPtr();

  base::test::TestFuture<void> command2_complete;
  auto command2 = std::make_unique<StrictMock<MockCommand<AppLock>>>(
      AppLockDescription(kTestAppId), command2_complete.GetCallback());
  base::WeakPtr<MockCommand<AppLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  CheckCommandsRunInOrder(command1_ptr, command1_complete, command2_ptr,
                          command2_complete);
}

TEST_F(WebAppCommandManagerTest, GlobalQueue) {
  base::test::TestFuture<void> command1_complete;
  auto command1 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      AllAppsLockDescription(), command1_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command1_ptr = command1->AsWeakPtr();

  base::test::TestFuture<void> command2_complete;
  auto command2 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      AllAppsLockDescription(), command2_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  CheckCommandsRunInOrder(command1_ptr, command1_complete, command2_ptr,
                          command2_complete);
}

TEST_F(WebAppCommandManagerTest, BackgroundWebContentsQueue) {
  base::test::TestFuture<void> command1_complete;
  auto command1 =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          SharedWebContentsLockDescription(), command1_complete.GetCallback());
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command1_ptr =
      command1->AsWeakPtr();

  base::test::TestFuture<void> command2_complete;
  auto command2 =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          SharedWebContentsLockDescription(), command2_complete.GetCallback());
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command2_ptr =
      command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  CheckCommandsRunInOrder(command1_ptr, command1_complete, command2_ptr,
                          command2_complete);
}

TEST_F(WebAppCommandManagerTest, ShutdownPreStartCommand) {
  base::test::TestFuture<void> on_command_complete;
  auto command = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      AllAppsLockDescription(), on_command_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr = command->AsWeakPtr();
  manager().ScheduleCommand(std::move(command));
  EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
  manager().Shutdown();
  EXPECT_TRUE(on_command_complete.Wait());
}

TEST_F(WebAppCommandManagerTest, ShutdownStartedCommand) {
  base::test::TestFuture<void> command_complete;
  auto mock_command = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      AllAppsLockDescription(), command_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr =
      mock_command->AsWeakPtr();

  manager().ScheduleCommand(std::move(mock_command));
  ASSERT_TRUE(command_ptr);
  EXPECT_FALSE(command_ptr->IsStarted());
  {
    base::RunLoop loop;
    EXPECT_CALL(*command_ptr, StartAfterStoringLock()).WillOnce([&]() {
      loop.Quit();
    });
    loop.Run();
  }
  testing::InSequence in_sequence;
  EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
  manager().Shutdown();
  ASSERT_TRUE(command_complete.Wait());
  EXPECT_FALSE(command_ptr);
}

TEST_F(WebAppCommandManagerTest, ScheduleAfterShutdown) {
  base::test::TestFuture<void> command_complete;
  auto mock_command = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      AllAppsLockDescription(), command_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command_ptr =
      mock_command->AsWeakPtr();
  manager().Shutdown();
  EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
  manager().ScheduleCommand(std::move(mock_command));
  EXPECT_TRUE(command_complete.Wait());
  ASSERT_FALSE(command_ptr);
}

TEST_F(WebAppCommandManagerTest, ShutdownQueuedCommand) {
  base::test::TestFuture<void> command1_complete;
  auto command1 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      AllAppsLockDescription(), command1_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command1_ptr = command1->AsWeakPtr();

  base::test::TestFuture<void> command2_complete;
  auto command2 = std::make_unique<StrictMock<MockCommand<AllAppsLock>>>(
      AllAppsLockDescription(), command2_complete.GetCallback());
  base::WeakPtr<MockCommand<AllAppsLock>> command2_ptr = command2->AsWeakPtr();

  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  {
    base::RunLoop loop;
    EXPECT_CALL(*command1_ptr, StartAfterStoringLock).WillOnce([&]() {
      loop.Quit();
    });
    loop.Run();
  }
  EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);
  EXPECT_CALL(*command2_ptr, OnDestruction()).Times(1);
  manager().Shutdown();
  ASSERT_TRUE(command1_complete.Wait());
  ASSERT_TRUE(command2_complete.Wait());
}

TEST_F(WebAppCommandManagerTest, MultipleSingleArgCallbackCommands) {
  base::RunLoop loop;
  // Queue multiple callbacks to app queues, and gather output.
  auto barrier = base::BarrierCallback<std::string>(
      2, base::BindLambdaForTesting([&](std::vector<std::string> result) {
        EXPECT_EQ(result.size(), 2u);
        loop.Quit();
      }));
  for (auto* app_id : {kTestAppId, kTestAppId2}) {
    base::OnceCallback<std::string(AppLock&, base::Value::Dict&)> callback =
        base::BindOnce([](webapps::AppId app_id, AppLock&,
                          base::Value::Dict&) { return app_id; },
                       app_id);
    manager().ScheduleCommand(
        std::make_unique<
            internal::CallbackCommandWithResult<AppLock, std::string>>(
            "", AppLockDescription(app_id), std::move(callback),
            /*on_complete=*/barrier, "shutdown"));
  }
  loop.Run();
}

TEST_F(WebAppCommandManagerTest, AppWithSharedWebContents) {
  base::test::TestFuture<void> command1_complete;
  auto command1 = std::make_unique<MockCommand<SharedWebContentsWithAppLock>>(
      SharedWebContentsWithAppLockDescription({kTestAppId}),
      command1_complete.GetCallback());
  base::test::TestFuture<void> command2_complete;
  auto command2 = std::make_unique<MockCommand<AppLock>>(
      AppLockDescription(kTestAppId), command2_complete.GetCallback());
  base::test::TestFuture<void> command3_complete;
  auto command3 = std::make_unique<MockCommand<SharedWebContentsLock>>(
      SharedWebContentsLockDescription(), command3_complete.GetCallback());
  base::WeakPtr<MockCommand<SharedWebContentsWithAppLock>> command1_ptr =
      command1->AsWeakPtr();
  base::WeakPtr<MockCommand<AppLock>> command2_ptr = command2->AsWeakPtr();
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command3_ptr =
      command3->AsWeakPtr();

  // One about:blank load per web contents lock.
  manager().ScheduleCommand(std::move(command1));
  manager().ScheduleCommand(std::move(command2));
  manager().ScheduleCommand(std::move(command3));
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*command1_ptr, StartAfterStoringLock())
        .Times(1)
        .WillOnce([&]() {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindLambdaForTesting([&]() {
                command1_ptr->CallCompleteAndSelfDestruct(
                    CommandResult::kSuccess);
              }));
        });
    EXPECT_CALL(*command2_ptr, StartAfterStoringLock()).Times(0);
    EXPECT_CALL(*command3_ptr, StartAfterStoringLock()).Times(0);
    EXPECT_CALL(*command1_ptr, OnDestruction()).Times(1);
    ASSERT_TRUE(command1_complete.Wait());
  }
  EXPECT_FALSE(command1_ptr);
  {
    EXPECT_CALL(*command2_ptr, StartAfterStoringLock())
        .Times(1)
        .WillOnce([&]() {
          command2_ptr->CallCompleteAndSelfDestruct(CommandResult::kSuccess);
        });
    EXPECT_CALL(*command3_ptr, StartAfterStoringLock())
        .Times(1)
        .WillOnce([&]() {
          command3_ptr->CallCompleteAndSelfDestruct(CommandResult::kSuccess);
        });
    EXPECT_CALL(*command2_ptr, OnDestruction()).Times(1);
    EXPECT_CALL(*command3_ptr, OnDestruction()).Times(1);
    ASSERT_TRUE(command2_complete.Wait());
    ASSERT_TRUE(command3_complete.Wait());
  }
  EXPECT_FALSE(command1_ptr);
  EXPECT_FALSE(command2_ptr);
  EXPECT_FALSE(command3_ptr);
}

TEST_F(WebAppCommandManagerTest, ToDebugValue) {
  base::test::TestFuture<void> on_command_complete;
  manager().ScheduleCommand(
      std::make_unique<internal::CallbackCommand<AppLock>>(
          "", AppLockDescription(kTestAppId),
          base::DoNothingAs<void(AppLock&, base::Value::Dict&)>(),
          on_command_complete.GetCallback()));
  manager().ScheduleCommand(
      std::make_unique<internal::CallbackCommand<AppLock>>(
          "", AppLockDescription(kTestAppId2),
          base::DoNothingAs<void(AppLock&, base::Value::Dict&)>(),
          base::DoNothing()));
  EXPECT_TRUE(on_command_complete.Wait());

  // Generally we should not test the output of the debug value, as this seems
  // fragile & can duplicate testing of the real functionality. However for the
  // common metadata it seems fine.
  base::Value::Dict command_manager_debug_value =
      manager().ToDebugValue().TakeDict();

  auto get_metadata_field_names =
      [](const base::Value::Dict& command_dict) -> std::vector<std::string> {
    std::vector<std::string> names;
    const base::Value::Dict* metadata = command_dict.FindDict("!metadata");
    std::transform(
        metadata->cbegin(), metadata->cend(), std::back_inserter(names),
        [](base::Value::Dict::const_iterator::reference pair) -> std::string {
          return pair.first;
        });
    return names;
  };

  base::Value::List* log = command_manager_debug_value.FindList("command_log");
  ASSERT_TRUE(log);
  ASSERT_GT(log->size(), 0ul);
  EXPECT_THAT(
      get_metadata_field_names(log->front().GetDict()),
      ::testing::UnorderedElementsAre(
          "command_result", "completion_location", "id", "initial_lock_request",
          "name", "result", "started", "scheduled_location"));

  base::Value::List* queue =
      command_manager_debug_value.FindList("command_queue");
  ASSERT_TRUE(queue);
  ASSERT_GT(queue->size(), 0ul);
  EXPECT_THAT(
      get_metadata_field_names(queue->front().GetDict()),
      ::testing::UnorderedElementsAre("id", "initial_lock_request", "name",
                                      "started", "scheduled_location"));
}

TEST_F(WebAppCommandManagerTest, DestroySharedWebContentsOnPostTask) {
  base::test::TestFuture<void> command_done;
  auto mock_command =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          SharedWebContentsLockDescription(), command_done.GetCallback());
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command_ptr =
      mock_command->AsWeakPtr();

  // Queue & complete a command with shared web contents.
  {
    testing::InSequence in_sequence;
    manager().ScheduleCommand(std::move(mock_command));
    ASSERT_TRUE(command_ptr);
    EXPECT_FALSE(command_ptr->IsStarted());
    EXPECT_CALL(*command_ptr, StartAfterStoringLock()).WillOnce([&]() {
      command_ptr->CallCompleteAndSelfDestruct(CommandResult::kSuccess);
    });
    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    ASSERT_TRUE(command_done.Wait());
  }
  // Validate web contents is destroyed.
  {
    // Verify the shared web contents is not destroyed yet.
    content::WebContents* web_contents = manager().web_contents_for_testing();
    EXPECT_TRUE(web_contents);
    // Wait for web contents to be destroyed.
    content::WebContentsDestroyedWatcher content_destroyed_observer(
        web_contents);
    content_destroyed_observer.Wait();
    // Verify the web contents is now destroyed.
    EXPECT_FALSE(manager().web_contents_for_testing());
  }
  EXPECT_FALSE(command_ptr);
}

TEST_F(WebAppCommandManagerTest, DestroySharedWebContentsOnShutdown) {
  base::test::TestFuture<void> command_done;
  auto mock_command =
      std::make_unique<StrictMock<MockCommand<SharedWebContentsLock>>>(
          SharedWebContentsLockDescription(), command_done.GetCallback());
  base::WeakPtr<MockCommand<SharedWebContentsLock>> command_ptr =
      mock_command->AsWeakPtr();

  // Queue and complete the command.
  {
    testing::InSequence in_sequence;
    manager().ScheduleCommand(std::move(mock_command));
    ASSERT_TRUE(command_ptr);
    EXPECT_FALSE(command_ptr->IsStarted());
    EXPECT_CALL(*command_ptr, StartAfterStoringLock()).WillOnce([&]() {
      command_ptr->CallCompleteAndSelfDestruct(CommandResult::kSuccess);
    });

    EXPECT_CALL(*command_ptr, OnDestruction()).Times(1);
    ASSERT_TRUE(command_done.Wait());
  }
  // Validate the web contents are not destroyed right away.
  {
    EXPECT_TRUE(manager().web_contents_for_testing());
    // Trigger shutdown and validate web contents is destroyed.
    manager().Shutdown();
    EXPECT_FALSE(manager().web_contents_for_testing());
  }
  EXPECT_FALSE(command_ptr);
}

}  // namespace
}  // namespace web_app
