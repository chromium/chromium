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
};

TEST_F(GlicActorTaskIconManagerTest, NoDuplicatedTaskNudgeStateUpdates) {
  MockTaskNudgeStateChangeSubscriber mock_subscriber;
  base::CallbackListSubscription subscription =
      manager()->RegisterTaskNudgeStateChange(base::BindRepeating(
          &MockTaskNudgeStateChangeSubscriber::OnStateChanged,
          base::Unretained(&mock_subscriber)));

  EXPECT_CALL(
      mock_subscriber,
      OnStateChanged(AllOf(Field(&ActorTaskNudgeState::text,
                                 ActorTaskNudgeState::Text::kNeedsAttention))));
  // Should only be one call for multiple tasks.
  EXPECT_CALL(mock_subscriber,
              OnStateChanged(AllOf(Field(
                  &ActorTaskNudgeState::text,
                  ActorTaskNudgeState::Text::kMultipleTasksNeedAttention))));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->GetTask(task_id_1)->Pause(/*from_actor=*/true);
  manager()->UpdateTaskListBubble(task_id_1);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);

  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->GetTask(task_id_2)->Pause(/*from_actor=*/true);
  manager()->UpdateTaskListBubble(task_id_2);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kMultipleTasksNeedAttention);

  TaskId task_id_3 = actor_service()->CreateTaskForTesting();
  actor_service()->GetTask(task_id_3)->Pause(/*from_actor=*/true);
  manager()->UpdateTaskListBubble(task_id_3);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kMultipleTasksNeedAttention);
}

TEST_F(GlicActorTaskIconManagerTest, NudgeShowsDefaultTextOnComplete) {
  MockTaskNudgeStateChangeSubscriber mock_subscriber;
  base::CallbackListSubscription subscription =
      manager()->RegisterTaskNudgeStateChange(base::BindRepeating(
          &MockTaskNudgeStateChangeSubscriber::OnStateChanged,
          base::Unretained(&mock_subscriber)));

  EXPECT_CALL(mock_subscriber, OnStateChanged(testing::_)).Times(0);

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
  MockTaskNudgeStateChangeSubscriber mock_nudge_subscriber;
  base::CallbackListSubscription nudge_subscription =
      manager()->RegisterTaskNudgeStateChange(base::BindRepeating(
          &MockTaskNudgeStateChangeSubscriber::OnStateChanged,
          base::Unretained(&mock_nudge_subscriber)));

  MockTaskListBubbleChangeSubscriber mock_bubble_subscriber;
  base::CallbackListSubscription bubble_subscription =
      manager()->RegisterTaskListBubbleStateChange(base::BindRepeating(
          &MockTaskListBubbleChangeSubscriber::OnStateChanged,
          base::Unretained(&mock_bubble_subscriber)));

  EXPECT_CALL(mock_nudge_subscriber,
              OnStateChanged(ActorTaskNudgeState{
                  .text = ActorTaskNudgeState::Text::kNeedsAttention}));
  EXPECT_CALL(mock_bubble_subscriber, OnStateChanged(actor::TaskId(1)));

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
  MockTaskNudgeStateChangeSubscriber mock_nudge_subscriber;
  base::CallbackListSubscription nudge_subscription =
      manager()->RegisterTaskNudgeStateChange(base::BindRepeating(
          &MockTaskNudgeStateChangeSubscriber::OnStateChanged,
          base::Unretained(&mock_nudge_subscriber)));

  MockTaskListBubbleChangeSubscriber mock_bubble_subscriber;
  base::CallbackListSubscription bubble_subscription =
      manager()->RegisterTaskListBubbleStateChange(base::BindRepeating(
          &MockTaskListBubbleChangeSubscriber::OnStateChanged,
          base::Unretained(&mock_bubble_subscriber)));

  EXPECT_CALL(mock_nudge_subscriber,
              OnStateChanged(ActorTaskNudgeState{
                  .text = ActorTaskNudgeState::Text::kNeedsAttention}));
  EXPECT_CALL(mock_bubble_subscriber, OnStateChanged(actor::TaskId(1)));
  EXPECT_CALL(mock_nudge_subscriber,
              OnStateChanged(ActorTaskNudgeState{
                  .text = ActorTaskNudgeState::Text::kDefault}));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  actor_service()->GetTask(task_id_1)->Pause(/*from_actor=*/true);

  manager()->UpdateTaskListBubble(task_id_1);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kNeedsAttention);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 1u);

  manager()->RemoveRowFromTaskListBubble(task_id_1);
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kDefault);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 0u);
}

TEST_F(GlicActorTaskIconManagerTest,
       MultipleTasksNeedAttentionNudgeShowsMultipleTasksText) {
  MockTaskNudgeStateChangeSubscriber mock_nudge_subscriber;
  base::CallbackListSubscription nudge_subscription =
      manager()->RegisterTaskNudgeStateChange(base::BindRepeating(
          &MockTaskNudgeStateChangeSubscriber::OnStateChanged,
          base::Unretained(&mock_nudge_subscriber)));

  MockTaskListBubbleChangeSubscriber mock_bubble_subscriber;
  base::CallbackListSubscription bubble_subscription =
      manager()->RegisterTaskListBubbleStateChange(base::BindRepeating(
          &MockTaskListBubbleChangeSubscriber::OnStateChanged,
          base::Unretained(&mock_bubble_subscriber)));

  EXPECT_CALL(mock_nudge_subscriber,
              OnStateChanged(ActorTaskNudgeState{
                  .text = ActorTaskNudgeState::Text::kNeedsAttention}));
  EXPECT_CALL(mock_bubble_subscriber, OnStateChanged(actor::TaskId(1)));
  EXPECT_CALL(
      mock_nudge_subscriber,
      OnStateChanged(ActorTaskNudgeState{
          .text = ActorTaskNudgeState::Text::kMultipleTasksNeedAttention}));
  EXPECT_CALL(mock_bubble_subscriber, OnStateChanged(actor::TaskId(2)));

  TaskId task_id_1 = actor_service()->CreateTaskForTesting();
  TaskId task_id_2 = actor_service()->CreateTaskForTesting();
  actor_service()->GetTask(task_id_1)->Pause(/*from_actor=*/true);
  actor_service()->GetTask(task_id_2)->Pause(/*from_actor=*/true);

  manager()->UpdateTaskListBubble(task_id_1);
  manager()->UpdateTaskNudge();
  manager()->UpdateTaskListBubble(task_id_2);
  manager()->UpdateTaskNudge();
  EXPECT_EQ(manager()->GetCurrentActorTaskNudgeState().text,
            ActorTaskNudgeState::Text::kMultipleTasksNeedAttention);
  EXPECT_EQ(manager()->GetActorTaskListBubbleRows().size(), 2u);
}
}  // namespace tabs
