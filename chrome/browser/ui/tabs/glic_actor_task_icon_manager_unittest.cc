// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {
using actor::ActorKeyedServiceFake;
using actor::TaskId;
using ActorTaskNudgeState = actor::ui::ActorTaskNudgeState;
using testing::AllOf;
using testing::Field;
using testing::ReturnRef;
using testing::Values;

class MockTaskNudgeStateChangeSubscriber {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (ActorTaskNudgeState actor_task_nudge_state));
};

class MockTaskListBubbleChangeSubscriber {
 public:
  MOCK_METHOD(void, OnStateChanged, ());
};

class GlicActorTaskIconManagerTest : public testing::Test,
                                     public testing::WithParamInterface<bool> {
 public:
  GlicActorTaskIconManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // testing::Test:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicActor,
         {{features::kGlicActorPolicyControlExemption.name, "true"}}}};
    feature_list_.InitWithFeaturesAndParameters(std::move(enabled_features),
                                                {});

    profile_ = std::make_unique<TestingProfile>();
    actor_service_ = std::make_unique<ActorKeyedServiceFake>(profile_.get());
    manager_ = std::make_unique<GlicActorTaskIconManager>(profile_.get(),
                                                          actor_service_.get());

    nudge_subscription_ = manager()->RegisterTaskNudgeStateChange(
        base::BindRepeating(&MockTaskNudgeStateChangeSubscriber::OnStateChanged,
                            base::Unretained(&mock_nudge_subscriber_)));

    bubble_subscription_ = manager()->RegisterTaskListBubbleStateChange(
        base::BindRepeating(&MockTaskListBubbleChangeSubscriber::OnStateChanged,
                            base::Unretained(&mock_bubble_subscriber_)));
  }

  void TearDown() override {
    manager_.reset();
    actor_service_->Shutdown();
    actor_service_.reset();
    profile_.reset();
    testing::Test::TearDown();
  }

  ActorKeyedServiceFake* actor_service() { return actor_service_.get(); }

  GlicActorTaskIconManager* manager() { return manager_.get(); }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ActorKeyedServiceFake> actor_service_;
  std::unique_ptr<GlicActorTaskIconManager> manager_;
  base::CallbackListSubscription nudge_subscription_;
  base::CallbackListSubscription bubble_subscription_;
  MockTaskNudgeStateChangeSubscriber mock_nudge_subscriber_;
  MockTaskListBubbleChangeSubscriber mock_bubble_subscriber_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlicActorTaskIconManagerTest, DefaultState) {
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest, NoActiveTasks_ReturnDefaultState) {
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest, NoDuplicatedTaskNudgeStateUpdates) {
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(AllOf(Field(&ActorTaskNudgeState::text,
                                 ActorTaskNudgeState::Text::kNeedsAttention))));
  // Should only be one call for default.
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(AllOf(Field(&ActorTaskNudgeState::text,
                                 ActorTaskNudgeState::Text::kDefault))));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);
  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);

  actor_service()->StopTask(task_id_1,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);

  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->StopTask(task_id_2,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_2);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest, NudgeShowsDefaultTextOnComplete) {
  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(testing::_)).Times(0);

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->StopTask(task_id_1,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest,
       PausedTaskUpdatesNudgeAndBubbleSubscribers) {
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(ActorTaskNudgeState{
                  .text = ActorTaskNudgeState::Text::kNeedsAttention}));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged());

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);

  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 1u);
  EXPECT_EQ(manager()->GetNumActorTasksNeedProcessing(), 1u);
}

TEST_F(GlicActorTaskIconManagerTest,
       ProcessingTaskInBubbleAlsoUpdatesTaskNudge) {
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(ActorTaskNudgeState{
                  .text = ActorTaskNudgeState::Text::kNeedsAttention}));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged());
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(ActorTaskNudgeState{
                  .text = ActorTaskNudgeState::Text::kDefault}));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);

  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 1u);

  manager()->ProcessRowInTaskListBubble(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 1u);
}

TEST_F(GlicActorTaskIconManagerTest,
       MultipleTasksNeedAttentionNudgeShowsMultipleTasksText) {
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged()).Times(2);

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_1);
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/false);
  manager()->OnActorTaskStateUpdate(task_id_2);

  manager()->UpdateTaskIconComponents(task_id_1);
  manager()->UpdateTaskIconComponents(task_id_2);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 2u);
  EXPECT_EQ(manager()->GetNumActorTasksNeedProcessing(), 1u);
}

TEST_F(GlicActorTaskIconManagerTest,
       MultipleTasksNeedAttentionRemainsInPopoverUntilAllClicked) {
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_1);
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_2);

  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_1), true);

  manager()->UpdateTaskIconComponents(task_id_2);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_2), true);

  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 2u);

  // Process one task, the text should remain the same and all bubbles should
  // still exist.
  manager()->ProcessRowInTaskListBubble(task_id_1);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_1), false);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 2u);

  // Process the other task, the text should change to default and all bubbles
  // should still exist.
  manager()->ProcessRowInTaskListBubble(task_id_2);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_2), false);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 2u);
}

TEST_F(GlicActorTaskIconManagerTest,
       OnActorTaskRemoved_RemovesTaskAndUpdatesBubbleAndNudge) {
  // Create a task.
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_1);

  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_1), true);

  // Stop task.
  actor_service()->StopTask(task_id_1,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  actor_service()->GetActorUiStateManager()->OnUiEvent(actor::ui::StopTask(
      task_id_1, actor::ActorTask::State::kFinished, "Test Task",
      /*last_acted_on_tab_handle=*/TabHandle()));
  task_environment().FastForwardBy(base::Seconds(
      features::kGlicActorUiCompletedTaskExpiryDelaySeconds.Get()));

  manager()->UpdateTaskIconComponents(task_id_1);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().size(), 0u);
}

TEST_F(GlicActorTaskIconManagerTest,
       OnActorTaskStopped_ProcessStoppedTasksAndUpdatesBubbleAndNudge) {
  // Create tasks.
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_1, /*from_actor=*/false);
  manager()->OnActorTaskStateUpdate(task_id_1);
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/true);
  manager()->OnActorTaskStateUpdate(task_id_2);
  TaskId task_id_3 = actor_service()->CreateTaskForTesting();
  actor_service()->GetActorUiStateManager()->OnUiEvent(
      actor::ui::TaskStateChanged(task_id_3, actor::ActorTask::State::kActing));
  manager()->OnActorTaskStateUpdate(task_id_3);

  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_1), false);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_2), true);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_3), false);

  actor_service()->StopTaskForTesting(
      task_id_1, actor::ActorTask::StoppedReason::kStoppedByUser);
  manager()->UpdateTaskIconComponents(task_id_1);
  actor_service()->StopTaskForTesting(
      task_id_2, actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_2);
  actor_service()->StopTaskForTesting(
      task_id_3, actor::ActorTask::StoppedReason::kChromeFailure);
  manager()->UpdateTaskIconComponents(task_id_3);
  EXPECT_FALSE(manager()->actor_task_list_bubble_rows().contains(task_id_1));
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_2), true);
  EXPECT_EQ(manager()->actor_task_list_bubble_rows().at(task_id_3), true);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kCompleteTasks);
}

TEST_F(GlicActorTaskIconManagerTest,
       NeedsAttentionNudgePrioritizesCompleteTasksNudge) {
  base::test::ScopedFeatureList scoped_features;
  // Create tasks.
  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->StopTaskForTesting(
      task_id_1, actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->UpdateTaskIconComponents(task_id_1);
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->PauseTaskForTesting(task_id_2, /*from_actor=*/true);
  manager()->UpdateTaskIconComponents(task_id_2);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
}

}  // namespace tabs
