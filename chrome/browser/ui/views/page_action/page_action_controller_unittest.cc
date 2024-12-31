// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_controller.h"

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

namespace page_actions {
namespace {

class PageActionTestObserver : public PageActionModelObserver {
 public:
  PageActionTestObserver() = default;
  ~PageActionTestObserver() override = default;

  void OnPageActionModelChanged(PageActionModel* model) override {
    ++model_changed_;
    show_requested_ = model->show_requested();
  }

  bool show_requested() const { return show_requested_; }
  int model_changed() const { return model_changed_; }

 private:
  bool show_requested_ = false;
  int model_changed_ = 0;
};

class PageActionControllerTest : public ::testing::Test {
 public:
  PageActionControllerTest() = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    pinned_actions_model_ =
        std::make_unique<PinnedToolbarActionsModel>(profile_.get());
    controller_ =
        std::make_unique<PageActionController>(pinned_actions_model_.get());
  }

  void TearDown() override {
    controller_.reset();
    pinned_actions_model_.reset();
    profile_.reset();
  }

  PageActionController* page_action_controller() { return controller_.get(); }
  PinnedToolbarActionsModel* pinned_actions_model() {
    return pinned_actions_model_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<PageActionController> controller_;
  std::unique_ptr<PinnedToolbarActionsModel> pinned_actions_model_;
  std::unique_ptr<TestingProfile> profile_;
};

// Tests adding/removing observers.
TEST_F(PageActionControllerTest, AddAndRemoveObserver) {
  auto observer = PageActionTestObserver();
  base::ScopedObservation<PageActionModel, PageActionModelObserver> observation(
      &observer);
  PageActionController* controller = page_action_controller();
  controller->Register(0);
  controller->AddObserver(0, observation);

  controller->Show(0);
  EXPECT_TRUE(observer.show_requested());

  observation.Reset();
  controller->Hide(0);
  EXPECT_TRUE(observer.show_requested());
}

// Tests that calling Show/HidePageAction will show/hide updates the model.
TEST_F(PageActionControllerTest, ShowAndHidePageAction) {
  auto observer = PageActionTestObserver();
  base::ScopedObservation<PageActionModel, PageActionModelObserver> observation(
      &observer);
  PageActionController* controller = page_action_controller();
  controller->Register(0);
  controller->AddObserver(0, observation);

  EXPECT_EQ(0, observer.model_changed());
  controller->Show(0);
  EXPECT_EQ(1, observer.model_changed());
  EXPECT_TRUE(observer.show_requested());

  controller->Show(0);
  EXPECT_EQ(1, observer.model_changed());
  EXPECT_TRUE(observer.show_requested());

  controller->Hide(0);
  EXPECT_EQ(2, observer.model_changed());
  EXPECT_FALSE(observer.show_requested());
}

// Tests that calling Show/HidePageAction will show/hide update the correct
// model.
TEST_F(PageActionControllerTest, ShowAndHidePageActionUpdatesCorrectModel) {
  auto observer_a = PageActionTestObserver();
  auto observer_b = PageActionTestObserver();
  base::ScopedObservation<PageActionModel, PageActionModelObserver>
      observation_a(&observer_a);
  base::ScopedObservation<PageActionModel, PageActionModelObserver>
      observation_b(&observer_b);
  PageActionController* controller = page_action_controller();

  controller->Initialize({0, 1});
  controller->AddObserver(0, observation_a);
  controller->AddObserver(1, observation_b);

  controller->Show(0);
  EXPECT_TRUE(observer_a.show_requested());
  EXPECT_FALSE(observer_b.show_requested());

  controller->Show(1);
  EXPECT_TRUE(observer_a.show_requested());
  EXPECT_TRUE(observer_b.show_requested());

  controller->Hide(0);
  EXPECT_FALSE(observer_a.show_requested());
  EXPECT_TRUE(observer_b.show_requested());
}

TEST_F(PageActionControllerTest, ShowIfNotPinned) {
  actions::ActionManager::Get().AddAction(
      actions::ActionItem::Builder().SetActionId(0).Build());
  PinnedToolbarActionsModel* pinned_actions = pinned_actions_model();

  auto observer = PageActionTestObserver();
  base::ScopedObservation<PageActionModel, PageActionModelObserver> observation(
      &observer);
  PageActionController* controller = page_action_controller();
  controller->Register(0);
  controller->AddObserver(0, observation);

  // Pin the action item. Nothing should happen.
  pinned_actions->UpdatePinnedState(0, true);
  EXPECT_FALSE(controller->ShowIfNotPinned(0));
  EXPECT_EQ(0, observer.model_changed());
  EXPECT_FALSE(observer.show_requested());

  // Unpin the action item. `ShowIfNotPinned` should take effect.
  pinned_actions->UpdatePinnedState(0, false);
  EXPECT_TRUE(controller->ShowIfNotPinned(0));
  EXPECT_EQ(1, observer.model_changed());
  EXPECT_TRUE(observer.show_requested());

  actions::ActionManager::Get().ResetForTesting();
}

}  // namespace
}  // namespace page_actions
