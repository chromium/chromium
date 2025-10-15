// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

#include "chrome/browser/actor/actor_keyed_service_fake.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/mock_glic_window_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {
using actor::ActorKeyedServiceFake;
using actor::TaskId;
using glic::GlicWindowController;
using glic::Host;
using glic::MockGlicWindowController;
using glic::mojom::CurrentView;
using testing::AllOf;
using testing::Field;
using testing::ReturnRef;
using testing::Values;

class MockTaskIconStateChangeSubscriber {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (bool is_showing,
               glic::mojom::CurrentView current_view,
               const ActorTaskIconState& actor_task_icon_state));
};

class GlicActorTaskIconManagerTest : public testing::Test {
 public:
  GlicActorTaskIconManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // testing::Test:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    actor_service_ = std::make_unique<ActorKeyedServiceFake>(profile_.get());
    window_controller_ = std::make_unique<MockGlicWindowController>();
    host_ = std::make_unique<Host>(profile_.get(), nullptr, nullptr,
                                   glic::GlicKeyedService::Get(profile_.get()));
    manager_ = std::make_unique<GlicActorTaskIconManager>(
        profile_.get(), actor_service_.get(), *window_controller_.get());
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
  std::unique_ptr<MockGlicWindowController> window_controller_;
  std::unique_ptr<Host> host_;
  std::unique_ptr<GlicActorTaskIconManager> manager_;
};

TEST_F(GlicActorTaskIconManagerTest, DefaultState) {
  EXPECT_FALSE(manager()->GetCurrentActorTaskIconState().is_visible);
  EXPECT_EQ(manager()->GetCurrentActorTaskIconState().text,
            ActorTaskIconState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest, NoActiveTasks_ReturnDefaultState) {
  manager()->UpdateTaskIcon(/*is_showing=*/true, CurrentView::kConversation);

  EXPECT_FALSE(manager()->GetCurrentActorTaskIconState().is_visible);
  EXPECT_EQ(manager()->GetCurrentActorTaskIconState().text,
            ActorTaskIconState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest, CancelledTask_ReturnDefaultText) {
  TaskId task_id = actor_service()->CreateTaskForTesting();
  actor_service()->StopTask(task_id, /*success=*/false);
  manager()->UpdateTaskIcon(/*is_showing=*/true, CurrentView::kConversation);

  EXPECT_FALSE(manager()->GetCurrentActorTaskIconState().is_visible);
  EXPECT_EQ(manager()->GetCurrentActorTaskIconState().text,
            ActorTaskIconState::Text::kDefault);
}

TEST_F(GlicActorTaskIconManagerTest,
       CompletedTaskAfterExpiry_ReturnDefaultState) {
  TaskId task_id = actor_service()->CreateTaskForTesting();
  actor_service()->StopTask(task_id, /*success=*/true);
  task_environment().FastForwardBy(base::Seconds(
      features::kGlicActorUiCompletedTaskExpiryDelaySeconds.Get()));
  manager()->UpdateTaskIcon(/*is_showing=*/true, CurrentView::kConversation);

  EXPECT_FALSE(manager()->GetCurrentActorTaskIconState().is_visible);
  EXPECT_EQ(manager()->GetCurrentActorTaskIconState().text,
            ActorTaskIconState::Text::kDefault);
}

class GlicActorTaskIconManagerPausedTasksTest
    : public GlicActorTaskIconManagerTest,
      public testing::WithParamInterface<
          std::tuple<bool, CurrentView, ActorTaskIconState::Text>> {
 public:
  // testing::Test:
  void SetUp() override {
    GlicActorTaskIconManagerTest::SetUp();
    actor::TaskId task_id = actor_service()->CreateTaskForTesting();
    actor_service()->GetTask(task_id)->Pause(/*from_actor=*/true);
  }
};

TEST_P(GlicActorTaskIconManagerPausedTasksTest, PausedTasks) {
  bool is_showing = std::get<0>(GetParam());
  CurrentView current_view = std::get<1>(GetParam());
  ActorTaskIconState::Text expected_text = std::get<2>(GetParam());

  MockTaskIconStateChangeSubscriber subscriber;
  base::CallbackListSubscription subscription =
      manager_->RegisterTaskIconStateChange(base::BindRepeating(
          &MockTaskIconStateChangeSubscriber::OnStateChanged,
          base::Unretained(&subscriber)));

  EXPECT_CALL(
      subscriber,
      OnStateChanged(is_showing, current_view,
                     AllOf(Field(&ActorTaskIconState::is_visible, true),
                           Field(&ActorTaskIconState::text, expected_text))));

  manager()->UpdateTaskIcon(is_showing, current_view);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicActorTaskIconManagerPausedTasksTest,
    Values(std::make_tuple(/*is_showing=*/false,
                           CurrentView::kConversation,
                           ActorTaskIconState::Text::kNeedsAttention),
           std::make_tuple(/*is_showing=*/true,
                           CurrentView::kConversation,
                           ActorTaskIconState::Text::kNeedsAttention)));

class GlicActorTaskIconManagerCompletedTasksTest
    : public GlicActorTaskIconManagerTest,
      public testing::WithParamInterface<
          std::tuple<bool, CurrentView, ActorTaskIconState::Text>> {
 public:
  // testing::Test:
  void SetUp() override {
    GlicActorTaskIconManagerTest::SetUp();
    TaskId task_id = actor_service()->CreateTaskForTesting();
    actor_service()->StopTask(task_id, /*success=*/true);
  }
};

TEST_P(GlicActorTaskIconManagerCompletedTasksTest, CompletedTasks) {
  bool is_showing = std::get<0>(GetParam());
  CurrentView current_view = std::get<1>(GetParam());
  ActorTaskIconState::Text expected_text = std::get<2>(GetParam());

  MockTaskIconStateChangeSubscriber subscriber;
  base::CallbackListSubscription subscription =
      manager_->RegisterTaskIconStateChange(base::BindRepeating(
          &MockTaskIconStateChangeSubscriber::OnStateChanged,
          base::Unretained(&subscriber)));

  EXPECT_CALL(
      subscriber,
      OnStateChanged(is_showing, current_view,
                     AllOf(Field(&ActorTaskIconState::is_visible, true),
                           Field(&ActorTaskIconState::text, expected_text))));

  manager()->UpdateTaskIcon(is_showing, current_view);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicActorTaskIconManagerCompletedTasksTest,
    testing::Values(std::make_tuple(/*is_showing=*/false,
                                    CurrentView::kConversation,
                                    ActorTaskIconState::Text::kCompleteTasks),
                    std::make_tuple(/*is_showing=*/true,
                                    CurrentView::kConversation,
                                    ActorTaskIconState::Text::kCompleteTasks)));

}  // namespace tabs
