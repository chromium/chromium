// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/actor/actor_autofill_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/actor/actor_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class MockCriticalActionService
    : public critical_actions::CriticalActionService {
 public:
  explicit MockCriticalActionService(const base::FilePath& db_path)
      : critical_actions::CriticalActionService(
            db_path,
            /*backend_task_runner=*/base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}
  ~MockCriticalActionService() override = default;

  MOCK_METHOD(void,
              AddCriticalAction,
              (const critical_actions::CriticalActionEntry& entry),
              (override));
  MOCK_METHOD(void,
              AddCriticalActionWithNavigationId,
              (const critical_actions::CriticalActionEntry& entry,
               int64_t navigation_id),
              (override));
};

class ActorAutofillManagerTest
    : public ::testing::Test,
      public WithTestAutofillClientDriverManager<TestActorAutofillClient> {
 public:
  ActorAutofillManagerTest() = default;

 protected:
  void SetUp() override {
    InitAutofillClient();
    CreateAutofillDriver();

    feature_list_.InitAndEnableFeature(
        critical_actions::features::kCriticalActionHistory);

    // sql::Database::Open does a DCHECK(!path.empty()) assertion that causes a
    // crash when database initialization is attempted with an empty path.
    // Since CriticalActionService always attempts to initialize the database
    // on startup, we must provide a valid temp file path to prevent crashes.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_path =
        temp_dir_.GetPath().AppendASCII("TestActorAutofill.db");

    mock_critical_service_ =
        std::make_unique<MockCriticalActionService>(db_path);
    manager_ = std::make_unique<ActorAutofillManager>(
        &autofill_client(), mock_critical_service_.get());
  }

  void TearDown() override {
    manager_.reset();
    mock_critical_service_.reset();
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  ActorAutofillManager& manager() { return *manager_; }
  MockCriticalActionService& mock_critical_service() {
    return *mock_critical_service_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<MockCriticalActionService> mock_critical_service_;
  std::unique_ptr<ActorAutofillManager> manager_;
};

// Test that when a form is filled and there is an active Actor task, a critical
// action of type kFormFill is logged to the CriticalActionService.
TEST_F(ActorAutofillManagerTest, OnFillOrPreviewFormLogsCriticalAction) {
  autofill_client().set_navigation_id(100);

  manager().set_active_actor_task(ActorAutofillManager::ActorTaskInfo{
      .conversation_id = "test-conv-id",
      .task_id = ::actor::TaskId(42),
  });

  FormData form = test::GetFormData({NAME_FIRST, EMAIL_ADDRESS});
  autofill_manager().AddSeenForm(form, {NAME_FIRST, EMAIL_ADDRESS});

  FormGlobalId form_id = form.global_id();
  FieldGlobalId field_id1 = form.fields()[0].global_id();
  FieldGlobalId field_id2 = form.fields()[1].global_id();

  EXPECT_CALL(
      mock_critical_service(),
      AddCriticalActionWithNavigationId(
          testing::AllOf(
              testing::Field(
                  &critical_actions::CriticalActionEntry::action_type,
                  critical_actions::ActionType::kFormFill),
              testing::Field(
                  &critical_actions::CriticalActionEntry::conversation_id,
                  "test-conv-id"),
              testing::Field(
                  &critical_actions::CriticalActionEntry::actor_task_id, "42")),
          100));

  manager().OnFillOrPreviewForm(
      autofill_manager(), form_id, field_id1, mojom::ActionPersistence::kFill,
      {field_id1, field_id2}, {}, static_cast<const AutofillProfile*>(nullptr));
}

// Test that when a single field is filled and there is an active Actor task, a
// critical action of type kFormFill is logged to the CriticalActionService.
TEST_F(ActorAutofillManagerTest, OnFillOrPreviewFieldLogsCriticalAction) {
  autofill_client().set_navigation_id(100);

  manager().set_active_actor_task(ActorAutofillManager::ActorTaskInfo{
      .conversation_id = "test-conv-id",
      .task_id = ::actor::TaskId(42),
  });

  FormData form = test::GetFormData({NAME_FIRST});
  autofill_manager().AddSeenForm(form, {NAME_FIRST});

  FormGlobalId form_id = form.global_id();
  FieldGlobalId field_id1 = form.fields()[0].global_id();

  EXPECT_CALL(
      mock_critical_service(),
      AddCriticalActionWithNavigationId(
          testing::AllOf(
              testing::Field(
                  &critical_actions::CriticalActionEntry::action_type,
                  critical_actions::ActionType::kFormFill),
              testing::Field(
                  &critical_actions::CriticalActionEntry::conversation_id,
                  "test-conv-id"),
              testing::Field(
                  &critical_actions::CriticalActionEntry::actor_task_id, "42")),
          100));

  manager().OnFillOrPreviewField(autofill_manager(), form_id, field_id1,
                                 mojom::ActionPersistence::kFill, u"value",
                                 NAME_FIRST);
}

// Test that when a field is filled but there is NO active Actor task, no
// critical action is logged to the CriticalActionService.
TEST_F(ActorAutofillManagerTest, NoCriticalActionLoggedIfNoActiveTask) {
  manager().set_active_actor_task(std::nullopt);

  FormData form = test::GetFormData({NAME_FIRST});
  autofill_manager().AddSeenForm(form, {NAME_FIRST});

  FormGlobalId form_id = form.global_id();
  FieldGlobalId field_id1 = form.fields()[0].global_id();

  EXPECT_CALL(mock_critical_service(), AddCriticalAction).Times(0);
  EXPECT_CALL(mock_critical_service(), AddCriticalActionWithNavigationId)
      .Times(0);

  manager().OnFillOrPreviewField(autofill_manager(), form_id, field_id1,
                                 mojom::ActionPersistence::kFill, u"value",
                                 NAME_FIRST);
}

}  // namespace

}  // namespace autofill
