// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_controller.h"

#include <memory>

#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_actions {
namespace {

class PageActionTestObserver : public PageActionModelObserver {
 public:
  PageActionTestObserver() = default;
  ~PageActionTestObserver() override = default;

  void OnVisibleChanged(bool is_visible) override {
    ++visible_changed_;
    is_visible_ = is_visible;
  }

  bool is_visible() const { return is_visible_; }
  int visible_changed() const { return visible_changed_; }

 private:
  bool is_visible_ = false;
  int visible_changed_ = 0;
};

class PageActionControllerTest : public ::testing::Test {
 public:
  PageActionControllerTest() = default;

  void SetUp() override {
    controller_ = std::make_unique<PageActionController>();
  }

  void TearDown() override { controller_.reset(); }

  PageActionController* page_action_controller() { return controller_.get(); }

 private:
  std::unique_ptr<PageActionController> controller_;
};

// Tests adding/removing observers.
TEST_F(PageActionControllerTest, AddAndRemoveObserver) {
  auto observer = PageActionTestObserver();
  PageActionController* controller = page_action_controller();
  controller->Register(0);
  controller->AddObserver(0, &observer);

  controller->Show(0);
  EXPECT_TRUE(observer.is_visible());

  controller->RemoveObserver(0, &observer);
  controller->Hide(0);
  EXPECT_TRUE(observer.is_visible());
}

// Tests that calling Show/HidePageAction will show/hide updates the model.
TEST_F(PageActionControllerTest, ShowAndHidePageAction) {
  auto observer = PageActionTestObserver();
  PageActionController* controller = page_action_controller();
  controller->Register(0);
  controller->AddObserver(0, &observer);

  EXPECT_EQ(0, observer.visible_changed());
  controller->Show(0);
  EXPECT_EQ(1, observer.visible_changed());
  EXPECT_TRUE(observer.is_visible());

  controller->Show(0);
  EXPECT_EQ(1, observer.visible_changed());
  EXPECT_TRUE(observer.is_visible());

  controller->Hide(0);
  EXPECT_EQ(2, observer.visible_changed());
  EXPECT_FALSE(observer.is_visible());
}

// Tests that calling Show/HidePageAction will show/hide update the correct
// model.
TEST_F(PageActionControllerTest, ShowAndHidePageActionUpdatesCorrectModel) {
  auto observer_a = PageActionTestObserver();
  auto observer_b = PageActionTestObserver();
  PageActionController* controller = page_action_controller();

  controller->Initialize({0, 1});
  controller->AddObserver(0, &observer_a);
  controller->AddObserver(1, &observer_b);

  controller->Show(0);
  EXPECT_TRUE(observer_a.is_visible());
  EXPECT_FALSE(observer_b.is_visible());

  controller->Show(1);
  EXPECT_TRUE(observer_a.is_visible());
  EXPECT_TRUE(observer_b.is_visible());

  controller->Hide(0);
  EXPECT_FALSE(observer_a.is_visible());
  EXPECT_TRUE(observer_b.is_visible());
}

}  // namespace
}  // namespace page_actions
