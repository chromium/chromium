// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "base/version_info/channel.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/internal/composite_context_decorator.h"
#include "components/contextual_tasks/internal/contextual_tasks_service_impl.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

using ::testing::_;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

MATCHER_P2(UrlAttachmentEq, url, title, "") {
  return arg.GetURL() == url && base::UTF16ToUTF8(arg.GetTitle()) == title;
}

class MockAimEligibilityService : public AimEligibilityService {
 public:
  explicit MockAimEligibilityService(PrefService* pref_service)
      : AimEligibilityService(*pref_service,
                              nullptr,
                              nullptr,
                              nullptr,
                              "en-US",
                              {}) {}
  MOCK_METHOD(bool, IsAimEligible, (), (const, override));

  // The following methods are marked as pure virtual in AimEligibilityService,
  // as they are implemented in ChromeAimEligibilityService which is the one
  // provided by the KeyedService factory. We therefore need to implement them
  // in this unit test.
  std::string GetCountryCode() const override { return "US"; }
  std::string GetLocale() const override { return "en-US"; }
};

class MockAiThreadSyncBridge : public AiThreadSyncBridge {
 public:
  MockAiThreadSyncBridge()
      : AiThreadSyncBridge(
            std::make_unique<
                testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>>(),
            syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest()) {}
  ~MockAiThreadSyncBridge() override = default;

  MOCK_METHOD(std::optional<Thread>,
              GetThread,
              (const std::string& server_id),
              (const, override));
  MOCK_METHOD(std::vector<Thread>, GetThreads, (), (const, override));
};

class MockContextualTasksObserver : public ContextualTasksService::Observer {
 public:
  MOCK_METHOD(void, OnContextualTasksServiceInitialized, (), (override));
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
  MOCK_METHOD(void,
              OnTaskAssociatedToTab,
              (const base::Uuid& task_id, SessionID tab_id),
              (override));
  MOCK_METHOD(void,
              OnTaskDisassociatedFromTab,
              (const base::Uuid& task_id, SessionID tab_id),
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
               std::unique_ptr<ContextDecorationParams> params,
               base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                   context_callback),
              (override));
};

class MockGetActiveTaskCountCallback {
 public:
  MOCK_METHOD(size_t, Run, ());
};

class ContextualTasksServiceImplTest : public testing::Test {
 public:
  ContextualTasksServiceImplTest() = default;
  ~ContextualTasksServiceImplTest() override = default;

  void SetUp() override {
    identity_test_environment_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);

    auto mock_decorator =
        std::make_unique<testing::NiceMock<MockCompositeContextDecorator>>();
    mock_decorator_ = mock_decorator.get();
    AimEligibilityService::RegisterProfilePrefs(pref_service_.registry());
    contextual_search::ContextualSearchService::RegisterProfilePrefs(
        pref_service_.registry());
    mock_aim_eligibility_service_ =
        std::make_unique<MockAimEligibilityService>(&pref_service_);
    service_ = std::make_unique<ContextualTasksServiceImpl>(
        version_info::Channel::UNKNOWN,
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        std::move(mock_decorator), mock_aim_eligibility_service_.get(),
        identity_test_environment_.identity_manager(), &pref_service_,
        SupportsEphemeralOnly(),
        base::BindRepeating(
            &MockGetActiveTaskCountCallback::Run,
            base::Unretained(&mock_get_active_task_count_callback_)));
  }

  virtual bool SupportsEphemeralOnly() { return false; }

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
        task_id, {}, nullptr,
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

  void CallOnThreadDataStoreLoaded() { service_->OnThreadDataStoreLoaded(); }

  void CallOnThreadAddedOrUpdatedRemotely(
      const std::vector<proto::AiThreadEntity>& threads) {
    service_->OnThreadAddedOrUpdatedRemotely(threads);
  }

  void CallOnThreadRemovedRemotely(const std::vector<base::Uuid>& thread_ids) {
    service_->OnThreadRemovedRemotely(thread_ids);
  }

  void SetAiThreadSyncBridgeForTesting(
      std::unique_ptr<AiThreadSyncBridge> bridge) {
    service_->SetAiThreadSyncBridgeForTesting(std::move(bridge));
  }

  void SetUpTaskWithThread(const base::Uuid& task_id,
                           ThreadType type,
                           const std::string& server_id,
                           const std::string& conversation_turn_id,
                           const std::string& title) {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->UpdateThreadForTask(task_id, ThreadType::kAiMode, server_id,
                                  conversation_turn_id, title);
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_environment_;
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service_;
  std::unique_ptr<ContextualTasksServiceImpl> service_;
  raw_ptr<testing::NiceMock<MockCompositeContextDecorator>> mock_decorator_;
  testing::NiceMock<MockContextualTasksObserver> observer_;
  testing::NiceMock<MockGetActiveTaskCountCallback>
      mock_get_active_task_count_callback_;
};

TEST_F(ContextualTasksServiceImplTest, CreateTask_RecordsActiveTasksHistogram) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(mock_get_active_task_count_callback_, Run()).WillOnce(Return(5));
  service_->CreateTask();
  histogram_tester.ExpectUniqueSample("ContextualTasks.ActiveTasksCount", 6, 1);
}

TEST_F(ContextualTasksServiceImplTest,
       CreateTaskFromUrl_RecordsActiveTasksHistogram) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(mock_get_active_task_count_callback_, Run()).WillOnce(Return(2));
  service_->CreateTaskFromUrl(GURL("https://google.com"));
  histogram_tester.ExpectUniqueSample("ContextualTasks.ActiveTasksCount", 3, 1);
}

TEST_F(ContextualTasksServiceImplTest, CreateTask_Persistent) {
  service_->AddObserver(&observer_);

  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskAdded(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  ContextualTask task = service_->CreateTask();
  run_loop.Run();
  EXPECT_TRUE(task.GetTaskId().is_valid());
  EXPECT_FALSE(task.IsEphemeral());

  std::vector<ContextualTask> tasks = GetTasks();
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(task.GetTaskId(), tasks[0].GetTaskId());

  service_->DeleteTask(task.GetTaskId());

  EXPECT_TRUE(GetTasks().empty());
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest,
       CreateTaskFromUrl_MatchesPrimaryAccount) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_environment_.SetCookieAccounts(
      {{primary_account_info.email, primary_account_info.gaia}});

  ContextualTask task =
      service_->CreateTaskFromUrl(GURL("https://google.com?authuser=0"));
  EXPECT_FALSE(task.IsEphemeral());
  EXPECT_EQ(1u, GetTasks().size());
}

TEST_F(ContextualTasksServiceImplTest,
       CreateTaskFromUrl_DoesNotMatchPrimaryAccount) {
  AccountInfo primary_account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  identity_test_environment_.SetCookieAccounts(
      {{"secondary@example.com",
        signin::GetTestGaiaIdForEmail("secondary@example.com")}});

  ContextualTask task =
      service_->CreateTaskFromUrl(GURL("https://google.com?authuser=0"));
  EXPECT_TRUE(task.IsEphemeral());
  EXPECT_EQ(0u, GetTasks().size());
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

TEST_F(ContextualTasksServiceImplTest, AssociateTabWithTask_Twice) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  EXPECT_EQ(1u, GetTasks().size());

  SessionID tab_id = SessionID::FromSerializedValue(1);

  // Associate a tab with a task.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnTaskAssociatedToTab(task.GetTaskId(), tab_id))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
    run_loop.Run();
    EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id));
    EXPECT_EQ(1u, service_->GetTabsAssociatedWithTask(task.GetTaskId()).size());
  }

  // Associate the same tab with the same task again without dissociating it.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnTaskAssociatedToTab(task.GetTaskId(), tab_id))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
    run_loop.Run();
    EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id));
    // Should not double count the same tab that got added.
    EXPECT_EQ(1u, service_->GetTabsAssociatedWithTask(task.GetTaskId()).size());
  }

  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AssociateTabWithDifferentTasks) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ContextualTask task2 = service_->CreateTask();
  EXPECT_EQ(2u, GetTasks().size());

  SessionID tab_id = SessionID::FromSerializedValue(1);

  // Associate a tab with a task.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnTaskAssociatedToTab(task.GetTaskId(), tab_id))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
    run_loop.Run();
    EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id));
    EXPECT_EQ(1u, service_->GetTabsAssociatedWithTask(task.GetTaskId()).size());
  }

  // Associate the same tab with a different task.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskDisassociatedFromTab(task.GetTaskId(), tab_id));
    EXPECT_CALL(observer_, OnTaskAssociatedToTab(task2.GetTaskId(), tab_id))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->AssociateTabWithTask(task2.GetTaskId(), tab_id);
    run_loop.Run();
    EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id));
    EXPECT_EQ(0u, service_->GetTabsAssociatedWithTask(task.GetTaskId()).size());
    EXPECT_EQ(1u,
              service_->GetTabsAssociatedWithTask(task2.GetTaskId()).size());
  }

  service_->RemoveObserver(&observer_);
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

TEST_F(ContextualTasksServiceImplTest, UpdateThreadForTask) {
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
  service_->UpdateThreadForTask(task.GetTaskId(), type, server_id,
                                conversation_turn_id, title);
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

  service_->UpdateThreadForTask(task1.GetTaskId(), type, server_id1,
                                conversation_turn_id1, title1);
  service_->UpdateThreadForTask(task2.GetTaskId(), type, server_id2,
                                conversation_turn_id2, title2);

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
    service_->UpdateThreadForTask(task.GetTaskId(), type, server_id,
                                  conversation_turn_id, title);
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

TEST_F(ContextualTasksServiceImplTest, UpdateThreadForTask_TaskDoesNotExist) {
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
  service_->UpdateThreadForTask(task_id, type, server_id, conversation_turn_id,
                                title);
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

TEST_F(ContextualTasksServiceImplTest, UpdateThreadForTask_UpdatesTurnId) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";
  std::string new_conversation_turn_id = "new_conversation_turn_id";

  // Add a thread to the task to set up the initial state.
  SetUpTaskWithThread(task.GetTaskId(), type, server_id, conversation_turn_id,
                      title);

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
    service_->UpdateThreadForTask(task.GetTaskId(), type, server_id,
                                  new_conversation_turn_id, std::nullopt);
    run_loop.Run();
  }

  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(new_conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, UpdateThreadForTask_UpdatesTitle) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string new_title = "bar";
  std::string conversation_turn_id = "conversation_turn_id";

  // Add a thread to the task to set up the initial state.
  SetUpTaskWithThread(task.GetTaskId(), type, server_id, conversation_turn_id,
                      title);

  // Update the thread's title and verify that the observer is notified
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
          EXPECT_EQ(thread->title, new_title);
          run_loop.Quit();
        });
    service_->UpdateThreadForTask(task.GetTaskId(), type, server_id,
                                  std::nullopt, new_title);
    run_loop.Run();
  }

  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(new_title, thread->title);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest,
       UpdateThreadForTask_UpdatesTitleAndTurnId) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string new_title = "bar";
  std::string conversation_turn_id = "conversation_turn_id";
  std::string new_conversation_turn_id = "new_conversation_turn_id";

  // Add a thread to the task to set up the initial state.
  SetUpTaskWithThread(task.GetTaskId(), type, server_id, conversation_turn_id,
                      title);

  // Update the thread's title and turn ID and verify that the observer is
  // notified with the correct data.
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
          EXPECT_EQ(thread->title, new_title);
          EXPECT_EQ(thread->conversation_turn_id, new_conversation_turn_id);
          run_loop.Quit();
        });
    service_->UpdateThreadForTask(task.GetTaskId(), type, server_id,
                                  new_conversation_turn_id, new_title);
    run_loop.Run();
  }

  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(new_title, thread->title);
  EXPECT_EQ(new_conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, GetTaskFromServerId) {
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";
  service_->UpdateThreadForTask(task.GetTaskId(), type, server_id,
                                conversation_turn_id, title);

  std::optional<ContextualTask> result =
      service_->GetTaskFromServerId(type, server_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(task.GetTaskId(), result->GetTaskId());
}

TEST_F(ContextualTasksServiceImplTest,
       UpdateThreadForTask_AvoidsDuplicateTask) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";
  // Initial call to create the task and add the thread.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(
        observer_,
        OnTaskAdded(testing::_, ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->UpdateThreadForTask(task.GetTaskId(), type, server_id,
                                  conversation_turn_id, title);
    run_loop.Run();
  }

  base::Uuid new_task_id = base::Uuid::GenerateRandomV4();
  std::string new_title = "bar";

  service_->UpdateThreadForTask(new_task_id, type, server_id, std::nullopt,
                                new_title);

  EXPECT_EQ(1u, GetTasks().size());
  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(new_title, thread->title);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest,
       UpdateThreadForTask_CreatesTaskIfNotFound) {
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
  service_->UpdateThreadForTask(task_id, type, server_id, conversation_turn_id,
                                std::nullopt);
  run_loop.Run();

  std::optional<ContextualTask> result = GetTaskById(task_id);
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(server_id, thread->server_id);
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, UpdateThreadForTask_ThreadDoesNotExist) {
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
  service_->UpdateThreadForTask(task.GetTaskId(), type, server_id,
                                conversation_turn_id, std::nullopt);
  run_loop.Run();

  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(server_id, thread->server_id);
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, UpdateThreadForTask_ServerIdMismatch) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  ThreadType type = ThreadType::kAiMode;
  std::string server_id = "server_id";
  std::string title = "foo";
  std::string conversation_turn_id = "conversation_turn_id";
  std::string new_conversation_turn_id = "new_conversation_turn_id";

  // Add a thread to the task to set up the initial state.
  SetUpTaskWithThread(task.GetTaskId(), type, server_id, conversation_turn_id,
                      title);

  // Attempt to update the thread with a wrong server ID and verify that the
  // observer is not notified.
  EXPECT_CALL(observer_, OnTaskUpdated(testing::_, testing::_)).Times(0);
  service_->UpdateThreadForTask(task.GetTaskId(), type, "wrong_server_id",
                                new_conversation_turn_id, std::nullopt);

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

TEST_F(ContextualTasksServiceImplTest, SetUrlResourcesFromServer) {
  service_->AddObserver(&observer_);

  ContextualTask task = service_->CreateTask();
  base::Uuid task_id = task.GetTaskId();

  // Setup existing resources
  // 1. Resource to be matched by ID (and updated)
  UrlResource res1(base::Uuid::GenerateRandomV4(),
                   GURL("https://example.com/1"));
  res1.title = "Old Title 1";

  // 2. Resource to be matched by Context ID (and filled)
  UrlResource res2(base::Uuid::GenerateRandomV4(),
                   GURL("https://example.com/2"));
  res2.context_id = 12345;
  res2.title = "Old Title 2";

  // 3. Resource to be matched by URL (and filled)
  UrlResource res3(base::Uuid::GenerateRandomV4(),
                   GURL("https://example.com/3"));
  res3.title = "Old Title 3";

  // 4. Resource to be removed
  UrlResource res4(base::Uuid::GenerateRandomV4(),
                   GURL("https://example.com/4"));

  std::vector<UrlResource> initial_resources = {res1, res2, res3, res4};

  // First call to set up initial state.
  // Expect adds for all.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->SetUrlResourcesFromServer(task_id, initial_resources);
    run_loop.Run();
  }

  // Now prepare incoming resources for the test.

  // 1. Update res1 (Match by ID)
  UrlResource in1(res1.url_id, GURL("https://example.com/1"));
  in1.title = "New Title 1";  // Changed

  // 2. Update res2 (Match by Context ID)
  UrlResource in2(GURL("https://example.com/2"),
                  ResourceType::kWebpage);  // No ID
  in2.context_id = 12345;
  // Missing title, should copy "Old Title 2"

  // 3. Update res3 (Match by URL)
  UrlResource in3(GURL("https://example.com/3"),
                  ResourceType::kWebpage);  // No ID, No Context ID
  // Missing title, should copy "Old Title 3"

  // 4. New resource (Added)
  UrlResource in5(GURL("https://example.com/5"), ResourceType::kWebpage);
  in5.title = "New Title 5";

  std::vector<UrlResource> incoming_resources = {in1, in2, in3, in5};

  base::RunLoop run_loop;

  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  service_->SetUrlResourcesFromServer(task_id, incoming_resources);
  run_loop.Run();

  // Verify final state
  std::optional<ContextualTask> result_task = GetTaskById(task_id);
  ASSERT_TRUE(result_task.has_value());
  std::vector<UrlResource> final_urls = result_task->GetUrlResources();
  ASSERT_EQ(4u, final_urls.size());  // 1, 2, 3, 5

  // Check 1
  auto it1 =
      std::find_if(final_urls.begin(), final_urls.end(),
                   [&](const auto& r) { return r.url_id == res1.url_id; });
  ASSERT_NE(it1, final_urls.end());
  EXPECT_EQ(it1->title, "New Title 1");

  // Check 2 (ID should be preserved)
  auto it2 =
      std::find_if(final_urls.begin(), final_urls.end(),
                   [&](const auto& r) { return r.url_id == res2.url_id; });
  ASSERT_NE(it2, final_urls.end());
  EXPECT_EQ(it2->title, "Old Title 2");  // Copied

  // Check 3 (ID should be preserved)
  auto it3 =
      std::find_if(final_urls.begin(), final_urls.end(),
                   [&](const auto& r) { return r.url_id == res3.url_id; });
  ASSERT_NE(it3, final_urls.end());
  EXPECT_EQ(it3->title, "Old Title 3");  // Copied

  // Check 5
  auto it5 = std::find_if(final_urls.begin(), final_urls.end(),
                          [&](const auto& r) { return r.url == in5.url; });
  ASSERT_NE(it5, final_urls.end());
  EXPECT_EQ(it5->title, "New Title 5");
  EXPECT_TRUE(it5->url_id.is_valid());

  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, SetUrlResourcesFromServer_NoChange) {
  service_->AddObserver(&observer_);

  ContextualTask task = service_->CreateTask();
  base::Uuid task_id = task.GetTaskId();

  UrlResource res1(base::Uuid::GenerateRandomV4(),
                   GURL("https://example.com/1"));
  res1.title = "Title 1";

  std::vector<UrlResource> initial_resources = {res1};

  // Setup initial state.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->SetUrlResourcesFromServer(task_id, initial_resources);
    run_loop.Run();
  }

  // Set the same resources again.
  // Expect NO calls to observer or bridge.
  EXPECT_CALL(observer_, OnTaskUpdated(testing::_, testing::_)).Times(0);
  service_->SetUrlResourcesFromServer(task_id, initial_resources);

  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, SetUrlResourcesFromServer_Reorder) {
  service_->AddObserver(&observer_);

  ContextualTask task = service_->CreateTask();
  base::Uuid task_id = task.GetTaskId();

  UrlResource res1(base::Uuid::GenerateRandomV4(),
                   GURL("https://example.com/1"));
  res1.title = "Title 1";
  UrlResource res2(base::Uuid::GenerateRandomV4(),
                   GURL("https://example.com/2"));
  res2.title = "Title 2";

  std::vector<UrlResource> initial_resources = {res1, res2};
  std::vector<UrlResource> reordered_resources = {res2, res1};

  // Setup initial state.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->SetUrlResourcesFromServer(task_id, initial_resources);
    run_loop.Run();
  }

  // Set the reordered resources.
  // Expect observer notification because order changed.
  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  service_->SetUrlResourcesFromServer(task_id, reordered_resources);
  run_loop.Run();

  std::optional<ContextualTask> result_task = GetTaskById(task_id);
  ASSERT_TRUE(result_task.has_value());
  std::vector<UrlResource> final_urls = result_task->GetUrlResources();
  ASSERT_EQ(2u, final_urls.size());
  EXPECT_EQ(final_urls[0].url, res2.url);
  EXPECT_EQ(final_urls[1].url, res1.url);

  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest,
       SetUrlResourcesFromServer_Deduplication) {
  service_->AddObserver(&observer_);

  ContextualTask task = service_->CreateTask();
  base::Uuid task_id = task.GetTaskId();

  // Resources for Phase 1.
  GURL url1("https://google.com");
  UrlResource r1(url1, ResourceType::kWebpage);
  r1.title = "Google";
  r1.url_id = base::Uuid::GenerateRandomV4();

  GURL url2("https://youtube.com");
  UrlResource r2(url2, ResourceType::kWebpage);
  r2.title = "YouTube";
  r2.url_id = base::Uuid::GenerateRandomV4();

  UrlResource r5(url1, ResourceType::kWebpage);
  r5.title = "Google Search";
  r5.url_id = base::Uuid::GenerateRandomV4();

  GURL pdf_url("file:///tmp/a.pdf");
  UrlResource r6(pdf_url, ResourceType::kPdf);
  r6.title = "A.pdf";
  r6.url_id = base::Uuid::GenerateRandomV4();

  UrlResource r7(pdf_url, ResourceType::kPdf);
  r7.title = "B.pdf";
  r7.url_id = base::Uuid::GenerateRandomV4();

  UrlResource r9(GURL(), ResourceType::kUnknown);
  r9.title = "Empty 1";
  r9.url_id = base::Uuid::GenerateRandomV4();

  UrlResource r10(GURL(), ResourceType::kUnknown);
  r10.title = "Empty 2";
  r10.url_id = base::Uuid::GenerateRandomV4();

  std::vector<UrlResource> initial_resources = {r1, r2, r5, r6, r7, r9, r10};

  // Phase 1: Set initial unique resources
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->SetUrlResourcesFromServer(task_id, initial_resources);
    run_loop.Run();
  }

  testing::Mock::VerifyAndClearExpectations(&observer_);

  // Verify Phase 1 state.
  std::optional<ContextualTask> result_task_p1 = GetTaskById(task_id);
  ASSERT_TRUE(result_task_p1.has_value());
  ASSERT_EQ(7u, result_task_p1->GetUrlResources().size());

  EXPECT_CALL(*mock_decorator_,
              DecorateContext(testing::_, testing::_, testing::_, testing::_))
      .WillRepeatedly(
          [](std::unique_ptr<ContextualTaskContext> context,
             const std::set<ContextualTaskContextSource>& sources,
             std::unique_ptr<ContextDecorationParams> params,
             base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                 callback) {
            // Mock decorator just passes the context through.
            std::move(callback).Run(std::move(context));
          });
  std::unique_ptr<ContextualTaskContext> context_p1 =
      GetContextForTask(task_id);
  ASSERT_TRUE(context_p1.get());
  EXPECT_EQ(context_p1->GetTaskId(), task_id);
  ASSERT_EQ(7u, context_p1->GetUrlAttachments().size());
  ASSERT_EQ(5u, context_p1->GetUniqueUrlAttachments().size());

  // Resources for Phase 2 (including duplicates and different types of
  // matches).
  UrlResource r3(url1, ResourceType::kWebpage);
  r3.url_id = r1.url_id;
  r3.title = "Google";  // Exact Duplicate of r1

  GURL url1_frag("https://google.com#frag");
  UrlResource r4(url1_frag, ResourceType::kWebpage);
  r4.title = "Google";  // Same Key as r1

  UrlResource r8(pdf_url, ResourceType::kPdf);
  r8.title = "A.pdf";  // Exact Duplicate of r6

  UrlResource r11(GURL(), ResourceType::kUnknown);
  r11.title = "Empty 1";  // Exact Duplicate of r9

  // Create an updated version of r2.
  UrlResource r2_updated(r2.url_id, url2);
  r2_updated.title = "YouTube Updated";  // Same key as r2

  std::vector<UrlResource> incoming_resources = {
      r1, r2_updated, r3, r4, r5, r6, r7, r8, r9, r10, r11};

  // Phase 2: Set resources again, including duplicates and updates.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_,
                OnTaskUpdated(testing::_,
                              ContextualTasksService::TriggerSource::kLocal))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    service_->SetUrlResourcesFromServer(task_id, incoming_resources);
    run_loop.Run();
  }

  // Verify final state.
  std::unique_ptr<ContextualTaskContext> context_p2 =
      GetContextForTask(task_id);
  ASSERT_TRUE(context_p2.get());
  EXPECT_EQ(context_p2->GetTaskId(), task_id);
  ASSERT_EQ(11u, context_p2->GetUrlAttachments().size());
  ASSERT_EQ(5u, context_p2->GetUniqueUrlAttachments().size());

  auto final_urls = context_p2->GetUniqueUrlAttachments();
  EXPECT_THAT(final_urls,
              UnorderedElementsAre(UrlAttachmentEq(url1, "Google"),
                                   UrlAttachmentEq(url2, "YouTube Updated"),
                                   UrlAttachmentEq(pdf_url, "A.pdf"),
                                   UrlAttachmentEq(GURL(), "Empty 1"),
                                   UrlAttachmentEq(GURL(), "Empty 2")));

  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, AssociateTabWithTask) {
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  SessionID tab_id = SessionID::FromSerializedValue(1);

  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnTaskAssociatedToTab(task.GetTaskId(), tab_id))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
  run_loop.Run();

  std::optional<ContextualTask> recent_task =
      service_->GetContextualTaskForTab(tab_id);
  ASSERT_TRUE(recent_task.has_value());
  EXPECT_EQ(task.GetTaskId(), recent_task->GetTaskId());
  service_->RemoveObserver(&observer_);
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
  service_->AddObserver(&observer_);
  ContextualTask task = service_->CreateTask();
  SessionID tab_id = SessionID::FromSerializedValue(1);

  service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
  EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id));

  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnTaskDisassociatedFromTab(task.GetTaskId(), tab_id))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service_->DisassociateTabFromTask(task.GetTaskId(), tab_id);
  run_loop.Run();
  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id));
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, GetContextualTaskForTab_NotFound) {
  SessionID tab_id = SessionID::FromSerializedValue(1);
  std::optional<ContextualTask> recent_task =
      service_->GetContextualTaskForTab(tab_id);
  EXPECT_FALSE(recent_task.has_value());
}

TEST_F(ContextualTasksServiceImplTest, DisassociateAllTabsFromTask) {
  service_->AddObserver(&observer_);

  // Wait for the sync pieces to finish init so there aren't multiple code
  // paths trying to add tasks to the service.
  base::RunLoop init_run_loop;
  EXPECT_CALL(observer_, OnContextualTasksServiceInitialized()).WillOnce([&]() {
    init_run_loop.Quit();
  });
  init_run_loop.Run();

  ContextualTask task = service_->CreateTask();
  SessionID tab_id1 = SessionID::FromSerializedValue(1);
  SessionID tab_id2 = SessionID::FromSerializedValue(2);

  service_->AssociateTabWithTask(task.GetTaskId(), tab_id1);
  service_->AssociateTabWithTask(task.GetTaskId(), tab_id2);

  EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id1).has_value());
  EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id2).has_value());
  std::optional<ContextualTask> result_task_before =
      GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result_task_before.has_value());
  EXPECT_EQ(2u, result_task_before->GetTabIds().size());

  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnTaskDisassociatedFromTab(task.GetTaskId(), tab_id1));
  EXPECT_CALL(observer_, OnTaskDisassociatedFromTab(task.GetTaskId(), tab_id2));
  EXPECT_CALL(observer_,
              OnTaskRemoved(task.GetTaskId(),
                            ContextualTasksService::TriggerSource::kLocal))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  service_->DisassociateAllTabsFromTask(task.GetTaskId());
  run_loop.Run();

  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id1).has_value());
  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id2).has_value());
  std::optional<ContextualTask> result_task_after =
      GetTaskById(task.GetTaskId());
  EXPECT_FALSE(result_task_after.has_value());
  EXPECT_EQ(0u, service_->GetTabIdMapSizeForTesting());
  std::vector<SessionID> tabs_for_task =
      service_->GetTabsAssociatedWithTask(task.GetTaskId());
  EXPECT_TRUE(tabs_for_task.empty());
  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id1));
  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id2));
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, GetContextForTask) {
  ContextualTask task = service_->CreateTask();
  GURL url("https://www.google.com");
  service_->AttachUrlToTask(task.GetTaskId(), url);

  EXPECT_CALL(*mock_decorator_,
              DecorateContext(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<ContextualTaskContext> context,
             const std::set<ContextualTaskContextSource>& sources,
             std::unique_ptr<ContextDecorationParams> params,
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
              DecorateContext(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [](std::unique_ptr<ContextualTaskContext> context,
             const std::set<ContextualTaskContextSource>& sources,
             std::unique_ptr<ContextDecorationParams> params,
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

// If there are threads provided by that backend but no tasks associated with
// them, the system should create one task per unowned thread.
TEST_F(ContextualTasksServiceImplTest,
       BuildContextualTasksFromLoadedData_NoPersistedTasks) {
  auto mock_ai_thread_bridge =
      std::make_unique<testing::NiceMock<MockAiThreadSyncBridge>>();

  std::string thread_id = "thread_id";

  // Only the thread for the first task is returned by the AiThreadSyncBridge.
  Thread thread(ThreadType::kAiMode, thread_id, "Thread Title",
                "conversation_turn_id");
  ON_CALL(*mock_ai_thread_bridge, GetThread(thread_id))
      .WillByDefault(Return(thread));
  ON_CALL(*mock_ai_thread_bridge, GetThreads())
      .WillByDefault(Return(std::vector<Thread>({thread})));

  SetAiThreadSyncBridgeForTesting(std::move(mock_ai_thread_bridge));

  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnContextualTasksServiceInitialized()).WillOnce([&]() {
    run_loop.Quit();
  });

  service_->AddObserver(&observer_);

  EXPECT_FALSE(service_->IsInitialized());
  CallOnThreadDataStoreLoaded();

  run_loop.Run();

  EXPECT_TRUE(service_->IsInitialized());

  // Since the task is being created based on the thread, the titles should be
  // the same.
  std::vector<ContextualTask> result_tasks = GetTasks();
  ASSERT_EQ(1u, result_tasks.size());
  std::optional<Thread> result_thread = result_tasks[0].GetThread();
  ASSERT_TRUE(result_thread.has_value());
  EXPECT_EQ(thread_id, result_thread->server_id);
  EXPECT_EQ(result_tasks[0].GetTitle(), result_thread->title);

  service_->RemoveObserver(&observer_);
}

// A task should not be created for a thread if it is already owned by a
// different task.
TEST_F(ContextualTasksServiceImplTest,
       BuildContextualTasksFromLoadedData_UnownedThread) {
  auto mock_ai_thread_bridge =
      std::make_unique<testing::NiceMock<MockAiThreadSyncBridge>>();

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  std::string thread_id = "thread_id";
  std::string thread_id_2 = "thread_id_2";

  // Add two threads where one isn't owned by a task.
  Thread thread(ThreadType::kAiMode, thread_id, "Thread 1",
                "conversation_turn_id");
  Thread thread_2(ThreadType::kAiMode, thread_id_2, "Thread 2",
                  "conversation_turn_id");

  ON_CALL(*mock_ai_thread_bridge, GetThread(thread_id))
      .WillByDefault(Return(thread));
  ON_CALL(*mock_ai_thread_bridge, GetThread(thread_id_2))
      .WillByDefault(Return(thread_2));
  ON_CALL(*mock_ai_thread_bridge, GetThreads())
      .WillByDefault(Return(std::vector<Thread>({thread, thread_2})));

  SetAiThreadSyncBridgeForTesting(std::move(mock_ai_thread_bridge));

  base::RunLoop run_loop;
  EXPECT_CALL(observer_, OnContextualTasksServiceInitialized()).WillOnce([&]() {
    run_loop.Quit();
  });

  service_->AddObserver(&observer_);

  EXPECT_FALSE(service_->IsInitialized());
  CallOnThreadDataStoreLoaded();

  run_loop.Run();

  EXPECT_TRUE(service_->IsInitialized());

  // There should be two tasks, one that was persisted and another created for
  // the unowned thread.
  std::vector<ContextualTask> result_tasks = GetTasks();
  ASSERT_EQ(2u, result_tasks.size());

  size_t thread_1_task_index = result_tasks[0].GetTitle() == "Thread 1" ? 0 : 1;
  size_t thread_2_task_index = thread_1_task_index == 1 ? 0 : 1;

  std::optional<Thread> result_thread_2 =
      result_tasks[thread_2_task_index].GetThread();
  ASSERT_TRUE(result_thread_2.has_value());
  EXPECT_EQ(thread_id_2, result_thread_2->server_id);
  EXPECT_EQ("Thread 2", result_tasks[thread_2_task_index].GetTitle());

  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, UpdateThreadForTask_ThreadTypeMismatch) {
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
    service_->UpdateThreadForTask(task.GetTaskId(), type, server_id,
                                  conversation_turn_id, title);
    run_loop.Run();
  }

  // Attempt to update the thread with a wrong thread type and verify that the
  // observer is not notified.
  EXPECT_CALL(observer_, OnTaskUpdated(testing::_, testing::_)).Times(0);
  service_->UpdateThreadForTask(task.GetTaskId(), ThreadType::kUnknown,
                                server_id, new_conversation_turn_id, title);

  std::optional<ContextualTask> result = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result.has_value());
  std::optional<Thread> thread = result->GetThread();
  ASSERT_TRUE(thread.has_value());
  EXPECT_EQ(conversation_turn_id, thread->conversation_turn_id);
  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, OnThreadAddedOrUpdatedRemotely) {
  service_->AddObserver(&observer_);

  // 1. Create a task with a thread and add it to the service.
  ContextualTask task = service_->CreateTask();
  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .WillOnce([&]() { run_loop.Quit(); });
  std::string server_id = "server_id_1";
  service_->UpdateThreadForTask(task.GetTaskId(), ThreadType::kAiMode,
                                server_id, "old_turn_id", "Old Title");
  run_loop.Run();

  // 2. Create an updated version of the thread.
  proto::AiThreadEntity updated_thread_entity;
  updated_thread_entity.mutable_specifics()->set_server_id(server_id);
  updated_thread_entity.mutable_specifics()->set_title("New Title");
  updated_thread_entity.mutable_specifics()->set_conversation_turn_id(
      "new_turn_id");
  updated_thread_entity.mutable_specifics()->set_type(
      sync_pb::AiThreadSpecifics::AI_MODE);

  // Add another thread with same server_id but different type.
  proto::AiThreadEntity updated_thread_entity_wrong_type;
  updated_thread_entity_wrong_type.mutable_specifics()->set_server_id(
      server_id);
  updated_thread_entity_wrong_type.mutable_specifics()->set_title(
      "Wrong Type Title");
  updated_thread_entity_wrong_type.mutable_specifics()
      ->set_conversation_turn_id("wrong_type_turn_id");
  updated_thread_entity_wrong_type.mutable_specifics()->set_type(
      sync_pb::AiThreadSpecifics::UNKNOWN);

  std::vector<proto::AiThreadEntity> updated_threads = {
      updated_thread_entity, updated_thread_entity_wrong_type};

  // 3. Expect OnTaskUpdated to be called and verify the changes.
  base::RunLoop run_loop2;
  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kRemote))
      .WillOnce([&](const ContextualTask& updated_task,
                    ContextualTasksService::TriggerSource source) {
        EXPECT_EQ(task.GetTaskId(), updated_task.GetTaskId());
        ASSERT_TRUE(updated_task.GetThread().has_value());
        EXPECT_EQ("New Title", updated_task.GetThread()->title);
        EXPECT_EQ("new_turn_id",
                  updated_task.GetThread()->conversation_turn_id);
        run_loop2.Quit();
      });

  // 4. Call the method under test.
  CallOnThreadAddedOrUpdatedRemotely(updated_threads);
  run_loop2.Run();

  // 5. Verify the task is updated in the service.
  std::optional<ContextualTask> result_task = GetTaskById(task.GetTaskId());
  ASSERT_TRUE(result_task.has_value());
  ASSERT_TRUE(result_task->GetThread().has_value());
  EXPECT_EQ("New Title", result_task->GetThread()->title);

  service_->RemoveObserver(&observer_);
}

// A task should be created for an added thread that isn't associated with a
// task.
TEST_F(ContextualTasksServiceImplTest, OnThreadAddedOrUpdatedRemotely_NoTask) {
  service_->AddObserver(&observer_);

  // 1. Create a thread to mimic a new one created from the backend.
  std::string server_id = "server_id";

  proto::AiThreadEntity thread_entity;
  thread_entity.mutable_specifics()->set_server_id(server_id);
  thread_entity.mutable_specifics()->set_title("Title");
  thread_entity.mutable_specifics()->set_conversation_turn_id("turn_id");
  thread_entity.mutable_specifics()->set_type(
      sync_pb::AiThreadSpecifics::AI_MODE);

  std::vector<proto::AiThreadEntity> new_threads = {thread_entity};

  // 2. Expect OnTaskAdded to be called and verify the task is created.
  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskAdded(testing::_, ContextualTasksService::TriggerSource::kRemote))
      .WillOnce([&](const ContextualTask& task,
                    ContextualTasksService::TriggerSource source) {
        EXPECT_EQ(task.GetTaskId(), task.GetTaskId());
        ASSERT_TRUE(task.GetThread().has_value());
        EXPECT_EQ("Title", task.GetThread()->title);
        EXPECT_EQ("turn_id", task.GetThread()->conversation_turn_id);
        run_loop.Quit();
      });

  // 3. Call the method under test.
  CallOnThreadAddedOrUpdatedRemotely(new_threads);
  run_loop.Run();

  // 4. Verify the task was created.
  EXPECT_EQ(1u, GetTasks().size());

  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, OnThreadRemovedRemotely) {
  service_->AddObserver(&observer_);

  base::RunLoop run_loop;
  EXPECT_CALL(
      observer_,
      OnTaskUpdated(testing::_, ContextualTasksService::TriggerSource::kLocal))
      .Times(2)
      .WillOnce(testing::Return())
      .WillOnce([&]() { run_loop.Quit(); });
  // 1. Create two tasks with threads.
  ContextualTask task_to_delete = service_->CreateTask();
  base::Uuid thread_id_to_delete = base::Uuid::GenerateRandomV4();
  service_->UpdateThreadForTask(task_to_delete.GetTaskId(), ThreadType::kAiMode,
                                thread_id_to_delete.AsLowercaseString(),
                                "turn_id_1", "Title 1");

  ContextualTask task_to_keep = service_->CreateTask();
  std::string thread_id_to_keep = "server_id_2";
  service_->UpdateThreadForTask(task_to_keep.GetTaskId(), ThreadType::kAiMode,
                                thread_id_to_keep, "turn_id_2", "Title 2");
  run_loop.Run();

  ASSERT_EQ(2u, GetTasks().size());

  // 2. Expect OnTaskRemoved to be called for the correct task.
  base::RunLoop run_loop2;
  EXPECT_CALL(observer_,
              OnTaskRemoved(task_to_delete.GetTaskId(),
                            ContextualTasksService::TriggerSource::kRemote))
      .WillOnce([&](const base::Uuid& task_id,
                    ContextualTasksService::TriggerSource source) {
        run_loop2.Quit();
      });

  // 3. Call the method under test to remove the first thread.
  CallOnThreadRemovedRemotely({thread_id_to_delete});
  run_loop2.Run();

  // 4. Verify that only the correct task was deleted.
  std::vector<ContextualTask> remaining_tasks = GetTasks();
  ASSERT_EQ(1u, remaining_tasks.size());
  EXPECT_EQ(task_to_keep.GetTaskId(), remaining_tasks[0].GetTaskId());

  service_->RemoveObserver(&observer_);
}

TEST_F(ContextualTasksServiceImplTest, GetTabsAssociatedWithTask) {
  ContextualTask task1 = service_->CreateTask();
  ContextualTask task2 = service_->CreateTask();

  SessionID tab_id1 = SessionID::FromSerializedValue(1);
  SessionID tab_id2 = SessionID::FromSerializedValue(2);
  SessionID tab_id3 = SessionID::FromSerializedValue(3);

  {
    base::HistogramTester histogram_tester;
    service_->AssociateTabWithTask(task1.GetTaskId(), tab_id1);
    histogram_tester.ExpectUniqueSample("ContextualTasks.TabAffiliationCount",
                                        1, 1);
  }
  {
    base::HistogramTester histogram_tester;
    service_->AssociateTabWithTask(task1.GetTaskId(), tab_id2);
    histogram_tester.ExpectUniqueSample("ContextualTasks.TabAffiliationCount",
                                        2, 1);
  }
  {
    base::HistogramTester histogram_tester;
    service_->AssociateTabWithTask(task2.GetTaskId(), tab_id3);
    histogram_tester.ExpectUniqueSample("ContextualTasks.TabAffiliationCount",
                                        1, 1);
  }

  std::vector<SessionID> tabs_for_task1 =
      service_->GetTabsAssociatedWithTask(task1.GetTaskId());
  ASSERT_EQ(2u, tabs_for_task1.size());
  EXPECT_TRUE(std::ranges::contains(tabs_for_task1, tab_id1));
  EXPECT_TRUE(std::ranges::contains(tabs_for_task1, tab_id2));

  std::vector<SessionID> tabs_for_task2 =
      service_->GetTabsAssociatedWithTask(task2.GetTaskId());
  ASSERT_EQ(1u, tabs_for_task2.size());
  EXPECT_TRUE(std::ranges::contains(tabs_for_task2, tab_id3));

  // Test with a task that has no associated tabs.
  ContextualTask task3 = service_->CreateTask();
  std::vector<SessionID> tabs_for_task3 =
      service_->GetTabsAssociatedWithTask(task3.GetTaskId());
  EXPECT_TRUE(tabs_for_task3.empty());

  // Test with an invalid task ID.
  base::Uuid invalid_task_id = base::Uuid::GenerateRandomV4();
  std::vector<SessionID> tabs_for_invalid_task =
      service_->GetTabsAssociatedWithTask(invalid_task_id);
  EXPECT_TRUE(tabs_for_invalid_task.empty());
}

TEST_F(ContextualTasksServiceImplTest,
       AssociateTabWithTask_DisassociatesOldTask) {
  ContextualTask task1 = service_->CreateTask();
  ContextualTask task2 = service_->CreateTask();
  SessionID tab_id = SessionID::FromSerializedValue(1);

  // Associate tab with task1.
  service_->AssociateTabWithTask(task1.GetTaskId(), tab_id);
  std::optional<ContextualTask> current_task =
      service_->GetContextualTaskForTab(tab_id);
  ASSERT_TRUE(current_task.has_value());
  EXPECT_EQ(task1.GetTaskId(), current_task->GetTaskId());
  EXPECT_TRUE(std::ranges::contains(
      service_->GetTabsAssociatedWithTask(task1.GetTaskId()), tab_id));
  EXPECT_FALSE(std::ranges::contains(
      service_->GetTabsAssociatedWithTask(task2.GetTaskId()), tab_id));

  // Associate same tab with task2.
  service_->AssociateTabWithTask(task2.GetTaskId(), tab_id);
  current_task = service_->GetContextualTaskForTab(tab_id);
  ASSERT_TRUE(current_task.has_value());
  EXPECT_EQ(task2.GetTaskId(), current_task->GetTaskId());
  EXPECT_TRUE(std::ranges::contains(
      service_->GetTabsAssociatedWithTask(task2.GetTaskId()), tab_id));
  EXPECT_FALSE(std::ranges::contains(
      service_->GetTabsAssociatedWithTask(task1.GetTaskId()), tab_id));
}

TEST_F(ContextualTasksServiceImplTest,
       DisassociateTabFromTask_RemovesEmptyTask) {
  ContextualTask task = service_->CreateTask();
  SessionID tab_id = SessionID::FromSerializedValue(1);

  // Associate the tab with the task.
  service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
  EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id).has_value());
  EXPECT_TRUE(GetTaskById(task.GetTaskId()).has_value());

  // Disassociate the tab. The task should be removed since it has no thread
  // and no other tabs.
  service_->DisassociateTabFromTask(task.GetTaskId(), tab_id);
  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id).has_value());
  EXPECT_FALSE(GetTaskById(task.GetTaskId()).has_value());
}

TEST_F(ContextualTasksServiceImplTest,
       DisassociateTabFromTask_KeepsEmptyTask_WhenFeatureDisabled) {
  feature_list_.InitAndDisableFeature(
      kContextualTasksRemoveTasksWithoutThreadsOrTabAssociations);
  ContextualTask task = service_->CreateTask();
  SessionID tab_id = SessionID::FromSerializedValue(1);

  // Associate the tab with the task.
  service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
  EXPECT_TRUE(service_->GetContextualTaskForTab(tab_id).has_value());
  EXPECT_TRUE(GetTaskById(task.GetTaskId()).has_value());

  // Disassociate the tab. The task should NOT be removed because the feature
  // is disabled.
  service_->DisassociateTabFromTask(task.GetTaskId(), tab_id);
  EXPECT_FALSE(service_->GetContextualTaskForTab(tab_id).has_value());
  EXPECT_TRUE(GetTaskById(task.GetTaskId()).has_value());
}

class ContextualTasksServiceImplEphemeralOnlyTest
    : public ContextualTasksServiceImplTest {
 public:
  ContextualTasksServiceImplEphemeralOnlyTest() = default;
  ~ContextualTasksServiceImplEphemeralOnlyTest() override = default;

  bool SupportsEphemeralOnly() override { return true; }
};

TEST_F(ContextualTasksServiceImplEphemeralOnlyTest, CreateTask) {
  ContextualTask task = service_->CreateTask();
  EXPECT_TRUE(task.IsEphemeral());
  EXPECT_TRUE(GetTasks().empty());
}

// Regression test for https://crbug.com/470110337
TEST_F(ContextualTasksServiceImplTest,
       AssociateTabWithTask_SelfAssociationCrash_crbug_470110337) {
  ContextualTask task = service_->CreateTask();
  SessionID tab_id = SessionID::FromSerializedValue(1);

  // 1. Associate tab.
  service_->AssociateTabWithTask(task.GetTaskId(), tab_id);

  // The original bug relied on the task being "empty" (no threads) so that
  // disassociation would trigger deletion. If this assertion fails in the
  // future, the test is no longer valid for this regression.
  ASSERT_FALSE(GetTaskById(task.GetTaskId())->GetThread().has_value());

  // 2. Re-associate same tab.
  // Previously, re-associating an empty task triggered a disassociate -> delete
  // cycle while holding an iterator, causing a Use-After-Free.
  service_->AssociateTabWithTask(task.GetTaskId(), tab_id);
}

}  // namespace contextual_tasks
