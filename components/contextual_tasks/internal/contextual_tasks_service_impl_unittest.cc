// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "base/version_info/channel.h"
#include "components/contextual_tasks/internal/composite_context_decorator.h"
#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

using ::testing::_;
using ::testing::Return;

class MockAimEligibilityService : public AimEligibilityService {
 public:
  explicit MockAimEligibilityService(PrefService* pref_service)
      : AimEligibilityService(*pref_service, nullptr, nullptr, nullptr) {}
  MOCK_METHOD(bool, IsAimEligible, (), (const, override));

  // The following methods are marked as pure virtual in AimEligibilityService,
  // as they are implemented in ChromeAimEligibilityService which is the one
  // provided by the KeyedService factory. We therefore need to implement them
  // in this unit test.
  std::string GetCountryCode() const override { return "US"; }
  std::string GetLocale() const override { return "en-US"; }
};

class MockContextualTasksObserver : public ContextualTasksService::Observer {
 public:
  MOCK_METHOD(void,
              OnTaskAdded,
              (const ContextualTask& task,
               ContextualTasksService::TriggerSource source),
              (override));
  MOCK_METHOD(void,
              OnTaskUpdated,
              (const ContextualTask& task,
               ContextualTasksService::TriggerSource source),
              (override));
  MOCK_METHOD(void,
              OnTaskRemoved,
              (const base::Uuid& task_id,
               ContextualTasksService::TriggerSource source),
              (override));
};

class MockCompositeContextDecorator : public CompositeContextDecorator {
 public:
  MockCompositeContextDecorator()
      : CompositeContextDecorator(
            std::map<ContextualTaskContextSource,
                     std::unique_ptr<ContextDecorator>>()) {}
  MOCK_METHOD(void,
              DecorateContext,
              (std::unique_ptr<ContextualTaskContext> context,
               const std::set<ContextualTaskContextSource>& sources,
               base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                   context_callback),
              (override));
};

class ContextualTasksServiceImplTest : public testing::Test {
 public:
  ContextualTasksServiceImplTest() {
    auto mock_decorator =
        std::make_unique<testing::NiceMock<MockCompositeContextDecorator>>();
    mock_decorator_ = mock_decorator.get();
    AimEligibilityService::RegisterProfilePrefs(pref_service_.registry());
    mock_aim_eligibility_service_ =
        std::make_unique<MockAimEligibilityService>(&pref_service_);
    service_ = std::make_unique<ContextualTasksServiceImpl>(
        version_info::Channel::UNKNOWN,
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        std::move(mock_decorator), mock_aim_eligibility_service_.get());
  }
  ~ContextualTasksServiceImplTest() override = default;

  std::vector<ContextualTask> GetTasks() {
    std::vector<ContextualTask> tasks;
    base::RunLoop run_loop;
    service_->GetTasks(base::BindOnce(
        [](std::vector<ContextualTask>* out_tasks,
           base::OnceClosure quit_closure, std::vector<ContextualTask> tasks) {
          *out_tasks = std::move(tasks);
          std::move(quit_closure).Run();
        },
        &tasks, run_loop.QuitClosure()));
    run_loop.Run();
    return tasks;
  }

  std::optional<ContextualTask> GetTaskById(const base::Uuid& task_id) {
    std::optional<ContextualTask> task;
    base::RunLoop run_loop;
    service_->GetTaskById(task_id,
                          base::BindOnce(
                              [](std::optional<ContextualTask>* out_task,
                                 base::OnceClosure quit_closure,
                                 std::optional<ContextualTask> result) {
                                *out_task = std::move(result);
                                std::move(quit_closure).Run();
                              },
                              &task, run_loop.QuitClosure()));
    run_loop.Run();
    return task;
  }

  std::unique_ptr<ContextualTaskContext> GetContextForTask(
      const base::Uuid& task_id) {
    std::unique_ptr<ContextualTaskContext> result;
    base::RunLoop run_loop;
    service_->GetContextForTask(
        task_id, {},
        base::BindOnce(
            [](std::unique_ptr<ContextualTaskContext>* out_context,
               base::OnceClosure quit_closure,
               std::unique_ptr<ContextualTaskContext> context) {
              *out_context = std::move(context);
              std::move(quit_closure).Run();
            },
            &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service_;
  std::unique_ptr<ContextualTasksServiceImpl> service_;
  raw_ptr<testing::NiceMock<MockCompositeContextDecorator>> mock_decorator_;
  testing::NiceMock<MockContextualTasksObserver> observer_;
};

TEST_F(ContextualTasksServiceImplTest, CreateTask) {
  service_->AddObserver(&observer_);

  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskAdded(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  ContextualTask task = service_->CreateTask();
  run_loop.Run();
  EXPECT_TRUE(task.GetTaskId().is_valid());

  std::vector<ContextualTask> tasks = GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task.GetTaskId(), tasks[0].GetTaskId());

  service_->DeleteTask(task.GetTaskId());

  EXPECT_TRUE(GetTasks().empty());
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, GetTaskById) {
  ContextualTask task = service_->CreateTask();
  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(task.GetTaskId(), result->GetTaskId());
}

TEST_F(ContextualTasksServiceImplTest, GetTaskById_NotFound) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  std::optional<ContextualTask> result = GetTaskById(task_id);
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContextualTasksServiceImplTest, CreateAndRemoveMultipleTasks) {
  ContextualTask task1 = service_->CreateTask();
  ContextualTask task2 = service_->CreateTask();
  EXPECT_EQ(2u, GetTasks().size());

  service_->DeleteTask(task1.GetTaskId());
  std::vector<ContextualTask> tasks = GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task2.GetTaskId(), tasks[0].GetTaskId());

  service_->DeleteTask(task2.GetTaskId());
  EXPECT_TRUE(GetTasks().empty());
}

TEST_F(ContextualTasksServiceImplTest, DeleteTask) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  EXPECT_EQ(1u, GetTasks().size());

  SessionID tab_id = SessionID::FromSerializedValue(1);
  service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
  EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id));

  base::RunLoop run_loop;
  EXPECT_CALL(observer_,
              OnTaskRemoved(task.GetTaskId(),
                            ContextualTasksService::TriggerSource::kLocal))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service_->DeleteTask(task.GetTaskId());
  run_loop.Run();
  EXPECT_TRUE(GetTasks().empty());
  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id));
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, DeleteTask_Twice) {
  ContextualTask task = service_->CreateTask();
  EXPECT_EQ(1u, GetTasks().size());
  service_->DeleteTask(task.GetTaskId());
  EXPECT_TRUE(GetTasks().empty());

  // Calling delete again should be a no-op and not crash.
  service_->DeleteTask(task.GetTaskId());
  EXPECT_TRUE(GetTasks().empty());
}

TEST_F(ContextualTasksServiceImplTest, DeleteTask_NotFound) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  service_->DeleteTask(task_id);
  EXPECT_TRUE(GetTasks().empty());
}

TEST_F(ContextualTasksServiceImplTest, GetTasks_Empty) {
  // A newly created service should have no tasks.
  EXPECT_TRUE(GetTasks().empty());
}

TEST_F(ContextualTasksServiceImplTest, AddThreadToTask) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";

  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service_->AddThreadToTask(
      task.GetTaskId(), Thread(type, server_id, title, conversation_turn_id));
  run_loop.Run();

  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(server_id, thread->server_id);
  EXPECT_EQ(type, thread->type);
  EXPECT_EQ(title, thread->title);
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AddAndRemoveThread_MultipleTasks) {
  ContextualTask task1 = service_->CreateTask();
  ContextualTask task2 = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id1 = "server_id1";
  std::string server_id2 = "server_id2";
  std::string title1 = "foo1";
  std::string title2 = "foo2";
  std::string conversation_turn_id1 = "conversation_turn_id1";
  std::string conversation_turn_id2 = "conversation_turn_id2";

  service_->AddThreadToTask(task1.GetTaskId(), Thread(type, server_id1, title1,
                                                      conversation_turn_id1));
  service_->AddThreadToTask(task2.GetTaskId(), Thread(type, server_id2, title2,
                                                      conversation_turn_id2));

  std::vector<ContextualTask> tasks_before_remove = GetTasks();
  ASSERT_EQ(2u, tasks_before_remove.size());

  ContextualTask result_task1_before =
      tasks_before_remove[0].GetTaskId() == task1.GetTaskId()
          ? tasks_before_remove[0]
          : tasks_before_remove[1];
  ContextualTask result_task2_before =
      tasks_before_remove[0].GetTaskId() == task2.GetTaskId()
          ? tasks_before_remove[0]
          : tasks_before_remove[1];

  std::optional<Thread> thread1 = result_task1_before.GetThread();
  ASSERT_TRUE(thread1.has_value());
  EXPECT_EQ(server_id1, thread1->server_id);
  EXPECT_EQ(title1, thread1->title);

  std::optional<Thread> thread2 = result_task2_before.GetThread();
  ASSERT_TRUE(thread2.has_value());
  EXPECT_EQ(server_id2, thread2->server_id);
  EXPECT_EQ(title2, thread2->title);

  service_->RemoveThreadFromTask(task1.GetTaskId(), type, server_id1);
  std::vector<ContextualTask> tasks_after_remove = GetTasks();
  ASSERT_EQ(1u, tasks_after_remove.size());

  EXPECT_TRUE(tasks_after_remove[0].GetThread().has_value());
}

TEST_F(ContextualTasksServiceImplTest, RemoveThreadFromTask) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->AddThreadToTask(
        task.GetTaskId(), Thread(type, server_id, title, conversation_turn_id));
    run_loop.Run();
  }

  std::vector<ContextualTask> tasks_before_remove = GetTasks();
  ASSERT_EQ(1u, tasks_before_remove.size());
  EXPECT_TRUE(tasks_before_remove[0].GetThread().has_value());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskRemoved(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->RemoveThreadFromTask(task.GetTaskId(), type, server_id);
    run_loop.Run();
  }
  std::vector<ContextualTask> tasks_after_remove = GetTasks();
  ASSERT_EQ(0u, tasks_after_remove.size());

  // Calling remove again should be a no-op and not crash.
  service_->RemoveThreadFromTask(task.GetTaskId(), type, server_id);
  std::vector<ContextualTask> tasks_after_second_remove = GetTasks();
  ASSERT_EQ(0u, tasks_after_remove.size());
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AddThreadToTask_TaskDoesNotExist) {
  service_->AddObserver(&observer_);
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";

  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskAdded(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service_->AddThreadToTask(
      task_id, Thread(type, server_id, title, conversation_turn_id));
  run_loop.Run();

  std::vector<ContextualTask> tasks = GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task_id, tasks[0].GetTaskId());
  std::optional<Thread> thread = tasks[0].GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(server_id, thread->server_id);
  EXPECT_EQ(type, thread->type);
  EXPECT_EQ(title, thread->title);
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, UpdateThreadTurnId) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";
  std::string new_conversation_turn_id = "new_conversation_turn_id";

  // Add a thread to the task to set up the initial state.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->AddThreadToTask(
        task.GetTaskId(), Thread(type, server_id, title, conversation_turn_id));
    run_loop.Run();
  }

  // Update the thread's turn ID and verify that the observer is notified
  // with the correct data.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce([&](const ContextualTask& updated_task,
                      ContextualTasksService::TriggerSource source) {
          EXPECT_EQ(updated_task.GetTaskId(), task.GetTaskId());
          std::optional<Thread> thread = updated_task.GetThread();
          ASSERT_TRUE(thread.has_value());
          EXPECT_EQ(thread->server_id, server_id);
          EXPECT_EQ(thread->conversation_turn_id, new_conversation_turn_id);
          run_loop.Quit();
        });
    service_->UpdateThreadTurnId(task.GetTaskId(), type, server_id,
                                 new_conversation_turn_id);
    run_loop.Run();
  }

  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(new_conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest,
       UpdateThreadTurnId_CreatesTaskIfNotFound) {
  service_->AddObserver(&observer_);
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string conversation_turn_id = "conversation_turn_id";

  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskAdded(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce([&](const ContextualTask& new_task,
                    ContextualTasksService::TriggerSource source) {
        EXPECT_EQ(new_task.GetTaskId(), task_id);
        std::optional<Thread> thread = new_task.GetThread();
        ASSERT_TRUE(thread.has_value());
        EXPECT_EQ(thread->server_id, server_id);
        EXPECT_EQ(thread->conversation_turn_id, conversation_turn_id);
        run_loop.Quit();
      });
  service_->UpdateThreadTurnId(task_id, type, server_id, conversation_turn_id);
  run_loop.Run();

  std::optional<ContextualTask> result = GetTaskById(task_id);
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(server_id, thread->server_id);
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, UpdateThreadTurnId_ThreadDoesNotExist) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string conversation_turn_id = "conversation_turn_id";

  // The task is created without a thread.
  ASSERT_FALSE(GetTaskById(task.GetTaskId())->GetThread().has_value());

  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce([&](const ContextualTask& updated_task,
                    ContextualTasksService::TriggerSource source) {
        EXPECT_EQ(updated_task.GetTaskId(), task.GetTaskId());
        std::optional<Thread> thread = updated_task.GetThread();
        ASSERT_TRUE(thread.has_value());
        EXPECT_EQ(thread->server_id, server_id);
        EXPECT_EQ(thread->conversation_turn_id, conversation_turn_id);
        EXPECT_EQ(thread->type, type);
        EXPECT_TRUE(thread->title.empty());
        run_loop.Quit();
      });
  service_->UpdateThreadTurnId(task.GetTaskId(), type, server_id,
                               conversation_turn_id);
  run_loop.Run();

  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(server_id, thread->server_id);
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, UpdateThreadTurnId_ServerIdMismatch) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";
  std::string new_conversation_turn_id = "new_conversation_turn_id";

  // Add a thread to the task to set up the initial state.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->AddThreadToTask(
        task.GetTaskId(), Thread(type, server_id, title, conversation_turn_id));
    run_loop.Run();
  }

  // Attempt to update the thread with a wrong server ID and verify that the
  // observer is not notified.
  EXPECT_CALL(observer_, OnTaskUpdated(testing::_, testing::_)).Times(0);
  service_->UpdateThreadTurnId(task.GetTaskId(), type, "wrong_server_id",
                               new_conversation_turn_id);

  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AttachUrlToTask) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  GURL url("https://www.google.com");

  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service_->AttachUrlToTask(task.GetTaskId(), url);
  run_loop.Run();

  std::vector<ContextualTask> tasks = GetTasks();
  ASSERT_EQ(1u, tasks.size());
  std::vector<UrlResource> urls = tasks[0].GetUrlResources();
  ASSERT_EQ(1u, urls.size());
  EXPECT_EQ(url, urls[0].url);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AttachAndDetachUrl_MultipleTasks) {
  ContextualTask task1 = service_->CreateTask();
  ContextualTask task2 = service_->CreateTask();
  GURL url1("https://www.google.com");
  GURL url2("https://www.youtube.com");

  service_->AttachUrlToTask(task1.GetTaskId(), url1);
  service_->AttachUrlToTask(task2.GetTaskId(), url2);

  std::vector<ContextualTask> tasks_before_detach = GetTasks();
  ASSERT_EQ(2u, tasks_before_detach.size());

  ContextualTask result_task1_before =
      tasks_before_detach[0].GetTaskId() == task1.GetTaskId()
          ? tasks_before_detach[0]
          : tasks_before_detach[1];
  ContextualTask result_task2_before =
      tasks_before_detach[0].GetTaskId() == task2.GetTaskId()
          ? tasks_before_detach[0]
          : tasks_before_detach[1];

  std::vector<UrlResource> urls1 = result_task1_before.GetUrlResources();
  ASSERT_EQ(1u, urls1.size());
  EXPECT_EQ(url1, urls1[0].url);

  std::vector<UrlResource> urls2 = result_task2_before.GetUrlResources();
  ASSERT_EQ(1u, urls2.size());
  EXPECT_EQ(url2, urls2[0].url);

  service_->DetachUrlFromTask(task1.GetTaskId(), url1);
  std::vector<ContextualTask> tasks_after_detach = GetTasks();
  ASSERT_EQ(2u, tasks_after_detach.size());
  EXPECT_TRUE(GetTaskById(task1.GetTaskId())->GetUrlResources().empty());
  EXPECT_EQ(1u, GetTaskById(task2.GetTaskId())->GetUrlResources().size());
}

TEST_F(ContextualTasksServiceImplTest, DetachUrlFromTask) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  GURL url("https://www.google.com");

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->AttachUrlToTask(task.GetTaskId(), url);
    run_loop.Run();
  }
  std::vector<ContextualTask> tasks_before_detach = GetTasks();
  EXPECT_EQ(1u, tasks_before_detach[0].GetUrlResources().size());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->DetachUrlFromTask(task.GetTaskId(), url);
    run_loop.Run();
  }
  std::vector<ContextualTask> tasks_after_detach = GetTasks();
  EXPECT_TRUE(tasks_after_detach[0].GetUrlResources().empty());
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AssociateTabWithTask) {
  ContextualTask task = service_->CreateTask();
  SessionID tab_id = SessionID::FromSerializedValue(1);

  service_->AssociateTabWithTask(task.GetTaskId(), tab_id);

  std::optional<ContextualTask> recent_task =
      service_->GetContextualTaskForTab(tab_id);
  ASSERT_TRUE(recent_task.has_value());
  EXPECT_EQ(task.GetTaskId(), recent_task->GetTaskId());
}

TEST_F(ContextualTasksServiceImplTest, AssociateTabWithInvalidTask) {
  ContextualTask task = service_->CreateTask();
  SessionID tab_id = SessionID::FromSerializedValue(1);
  base::Uuid task_id = task.GetTaskId();
  service_->DeleteTask(task_id);

  // The session Id is not added, as the task is deleted.
  service_->AssociateTabWithTask(task_id, tab_id);

  std::optional<ContextualTask> recent_task =
      service_->GetContextualTaskForTab(tab_id);
  EXPECT_EQ(0u, service_->GetTabIdMapSizeForTesting());
}

TEST_F(ContextualTasksServiceImplTest, DisassociateTabFromTask) {
  ContextualTask task = service_->CreateTask();
  SessionID tab_id = SessionID::FromSerializedValue(1);

  service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
  EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id));

  service_->DisassociateTabFromTask(task.GetTaskId(), tab_id);
  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id));
}

TEST_F(ContextualTasksServiceImplTest, GetContextualTaskForTab_NotFound) {
  SessionID tab_id = SessionID::FromSerializedValue(1);
  std::optional<ContextualTask> recent_task =
      service_->GetContextualTaskForTab(tab_id);
  EXPECT_FALSE(recent_task.has_value());
}

TEST_F(ContextualTasksServiceImplTest, ClearAllTabAssociationsForTask) {
  ContextualTask task = service_->CreateTask();
  SessionID tab_id1 = SessionID::FromSerializedValue(1);
  SessionID tab_id2 = SessionID::FromSerializedValue(2);

  service_->AssociateTabWithTask(task.GetTaskId(), tab_id1);
  service_->AssociateTabWithTask(task.GetTaskId(), tab_id2);

  EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id1).has_value());
  EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id2).has_value());
  EXPECT_EQ(2u, GetTaskById(task.GetTaskId())->GetTabIds().size());

  service_->ClearAllTabAssociationsForTask(task.GetTaskId());

  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id1).has_value());
  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id2).has_value());
  EXPECT_EQ(0u, GetTaskById(task.GetTaskId())->GetTabIds().size());
  EXPECT_EQ(0u, service_->GetTabIdMapSizeForTesting());
}

TEST_F(ContextualTasksServiceImplTest, GetContextForTask) {
  ContextualTask task = service_->CreateTask();
  GURL url("https://www.google.com");
  service_->AttachUrlToTask(task.GetTaskId(), url);

  EXPECT_CALL(*mock_decorator_,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<ContextualTaskContext> context,
             const std::set<ContextualTaskContextSource>& sources,
             base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                 callback) {
            // Mock decorator just passes the context through.
            std::move(callback).Run(std::move(context));
          });

  std::unique_ptr<ContextualTaskContext> context =
      GetContextForTask(task.GetTaskId());
  ASSERT_TRUE(context.get());
  EXPECT_EQ(context->GetTaskId(), task.GetTaskId());
  const auto& attachments = context->GetUrlAttachments();
  ASSERT_EQ(attachments.size(), 1u);
  EXPECT_EQ(attachments[0].GetURL(), url);
  EXPECT_TRUE(attachments[0].GetTitle().empty());
}

TEST_F(ContextualTasksServiceImplTest, GetContextForTask_WithTitle) {
  ContextualTask task = service_->CreateTask();
  GURL url("https://www.google.com");
  service_->AttachUrlToTask(task.GetTaskId(), url);

  EXPECT_CALL(*mock_decorator_,
              DecorateContext(testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<ContextualTaskContext> context,
             const std::set<ContextualTaskContextSource>& sources,
             base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                 callback) {
            // Mock decorator adds a title.
            (context->GetMutableUrlAttachmentsForTesting()[0]
                 .GetMutableDecoratorDataForTesting())
                .fallback_title_data.title = u"Hardcoded Title";
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback), std::move(context)));
          });

  std::unique_ptr<ContextualTaskContext> context =
      GetContextForTask(task.GetTaskId());
  ASSERT_TRUE(context.get());
  EXPECT_EQ(context->GetTaskId(), task.GetTaskId());
  const auto& attachments = context->GetUrlAttachments();
  ASSERT_EQ(attachments.size(), 1u);
  EXPECT_EQ(attachments[0].GetURL(), url);
  EXPECT_EQ(attachments[0].GetTitle(), u"Hardcoded Title");
}

TEST_F(ContextualTasksServiceImplTest, GetContextForTask_NotFound) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  std::unique_ptr<ContextualTaskContext> context = GetContextForTask(task_id);
  EXPECT_FALSE(context.get());
}

TEST_F(ContextualTasksServiceImplTest, GetFeatureEligibility) {
  // Test case 1: Feature flag enabled, AIM eligible.
  feature_list_.InitAndEnableFeature(kContextualTasks);
  EXPECT_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(true));
  EXPECT_TRUE(service_->GetFeatureEligibility().IsEligible());

  // Test case 2: Feature flag enabled, AIM not eligible.
  EXPECT_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(false));
  EXPECT_FALSE(service_->GetFeatureEligibility().IsEligible());

  feature_list_.Reset();
  // Test case 3: Feature flag disabled, AIM eligible.
  feature_list_.InitAndDisableFeature(kContextualTasks);
  EXPECT_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(true));
  EXPECT_FALSE(service_->GetFeatureEligibility().IsEligible());

  // Test case 4: Feature flag disabled, AIM not eligible.
  EXPECT_CALL(*mock_aim_eligibility_service_, IsAimEligible())
      .WillOnce(Return(false));
  EXPECT_FALSE(service_->GetFeatureEligibility().IsEligible());
}

}  // namespace contextual_tasks
