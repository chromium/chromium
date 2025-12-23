// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_task.h"
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
  MOCK_METHOD(void, OnStateChanged, (actor::TaskId task_id));
};

class GlicActorTaskIconManagerTest : public testing::Test {
 public:
  GlicActorTaskIconManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // testing::Test:
  void SetUp() override {
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
};

TEST_F(GlicActorTaskIconManagerTest, DefaultState) {
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest, NoActiveTasks_ReturnDefaultState) {
  manager()->UpdateTaskNudge();

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
  actor_service()->GetTask(task_id_1)->Pause(/*from_actor=*/true);
  manager()->UpdateTaskListBubble(task_id_1);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);

  actor_service()->StopTask(task_id_1,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->OnActorTaskStopped(task_id_1, actor::ActorTask::State::kFinished,
                                /*task_title=*/"");
  manager()->UpdateTaskListBubble(task_id_1);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);

  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->StopTask(task_id_2,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->OnActorTaskStopped(task_id_2, actor::ActorTask::State::kFinished,
                                /*task_title=*/"");
  manager()->UpdateTaskListBubble(task_id_2);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest, NudgeShowsDefaultTextOnComplete) {
  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(testing::_)).Times(0);

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->StopTask(task_id_1,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  manager()->OnActorTaskStopped(task_id_1, actor::ActorTask::State::kFinished,
                                /*task_title=*/"");
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest,
       PausedTaskUpdatesNudgeAndBubbleSubscribers) {
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(ActorTaskNudgeState{
                  .text = ActorTaskNudgeState::Text::kNeedsAttention,
                  .task_list_size = 1}));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(actor::TaskId(1)));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->GetTask(task_id_1)->Pause(/*from_actor=*/true);

  manager()->UpdateTaskListBubble(task_id_1);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 1u);
}

TEST_F(GlicActorTaskIconManagerTest,
       RemovingTaskFromBubbleAlsoUpdatesTaskNudge) {
  EXPECT_CALL(mock_nudge_subscriber_,
              OnStateChanged(ActorTaskNudgeState{
                  .text = ActorTaskNudgeState::Text::kNeedsAttention,
                  .task_list_size = 1}));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(actor::TaskId(1)));
  EXPECT_CALL(
      mock_nudge_subscriber_,
      OnStateChanged(ActorTaskNudgeState{
          .text = ActorTaskNudgeState::Text::kDefault, .task_list_size = 0}));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->GetTask(task_id_1)->Pause(/*from_actor=*/true);

  manager()->UpdateTaskListBubble(task_id_1);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 1u);

  manager()->ProcessRowInTaskListBubble(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 0u);
}

TEST_F(GlicActorTaskIconManagerTest,
       MultipleTasksNeedAttentionNudgeShowsMultipleTasksText) {
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(actor::TaskId(1)));
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(actor::TaskId(2)));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->GetTask(task_id_1)->Pause(/*from_actor=*/true);
  actor_service()->GetTask(task_id_2)->Pause(/*from_actor=*/true);

  manager()->UpdateTaskListBubble(task_id_1);
  manager()->UpdateTaskNudge();
  manager()->UpdateTaskListBubble(task_id_2);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 2u);
}

TEST_F(GlicActorTaskIconManagerTest,
       MultipleTasksNeedAttentionRemainsInPopoverUntilAllClicked) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      features::kGlicActorUiGlobalTaskIndicator, {});

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->GetTask(task_id_1)->Pause(/*from_actor=*/true);
  actor_service()->GetTask(task_id_2)->Pause(/*from_actor=*/true);

  manager()->UpdateTaskListBubble(task_id_1);
  EXPECT_EQ(
      manager()->GetActorTaskListBubbleRows()[task_id_1].requires_processing,
      true);
  manager()->UpdateTaskNudge();

  manager()->UpdateTaskListBubble(task_id_2);
  EXPECT_EQ(
      manager()->GetActorTaskListBubbleRows()[task_id_2].requires_processing,
      true);
  manager()->UpdateTaskNudge();

  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 2u);

  // Process one task, the text should remain the same and all bubbles should
  // still exist.
  manager()->ProcessRowInTaskListBubble(task_id_1);
  EXPECT_EQ(
      manager()->GetActorTaskListBubbleRows()[task_id_1].requires_processing,
      false);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 2u);

  // Process the other task, the text should change to default and all bubbles
  // should still exist.
  manager()->ProcessRowInTaskListBubble(task_id_2);
  EXPECT_EQ(
      manager()->GetActorTaskListBubbleRows()[task_id_2].requires_processing,
      false);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 2u);
}

TEST_F(GlicActorTaskIconManagerTest,
       OnActorTaskRemovedTriggersUpdateWhenGlobalTaskIndicatorEnabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(
      features::kGlicActorUiGlobalTaskIndicator);

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();

  actor_service()->GetTask(task_id_1)->Pause(/*from_actor=*/true);
  actor_service()->GetTask(task_id_2)->Pause(/*from_actor=*/true);

  manager()->OnActorTaskStateUpdate(task_id_1);
  manager()->OnActorTaskStateUpdate(task_id_2);
  testing::Mock::VerifyAndClearExpectations(&mock_bubble_subscriber_);
  testing::Mock::VerifyAndClearExpectations(&mock_nudge_subscriber_);

  // Expect OnActorTaskRemoved to only trigger updates to last updated task
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(task_id_1)).Times(0);
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(task_id_2));

  manager()->OnActorTaskRemoved(task_id_1);
}

TEST_F(GlicActorTaskIconManagerTest, OnActorTaskRemovedDoesNothingByDefault) {
  TaskId task_id = actor_service()->CreateTaskForTesting();
  manager()->OnActorTaskStateUpdate(task_id);
  testing::Mock::VerifyAndClearExpectations(&mock_bubble_subscriber_);
  testing::Mock::VerifyAndClearExpectations(&mock_nudge_subscriber_);

  // Expect OnActorTaskRemoved to NOT trigger updates
  EXPECT_CALL(mock_bubble_subscriber_, OnStateChanged(testing::_)).Times(0);
  EXPECT_CALL(mock_nudge_subscriber_, OnStateChanged(testing::_)).Times(0);

  manager()->OnActorTaskRemoved(task_id);
}

}  // namespace tabs
