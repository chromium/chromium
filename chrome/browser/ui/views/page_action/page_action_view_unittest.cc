// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include <memory>

#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/views/actions/action_view_controller.h"

namespace page_actions {
namespace {

class MockIconLabelViewDelegate : public IconLabelBubbleView::Delegate {
 public:
  MOCK_METHOD(SkColor,
              GetIconLabelBubbleSurroundingForegroundColor,
              (),
              (const, override));
  MOCK_METHOD(SkColor,
              GetIconLabelBubbleBackgroundColor,
              (),
              (const, override));
};

class PageActionViewTest : public ::testing::Test {
 public:
  PageActionViewTest() = default;

  void SetUp() override {
    action_item_ = actions::ActionManager::Get().AddAction(
        actions::ActionItem::Builder().SetActionId(0).Build());
    page_action_view_ = std::make_unique<PageActionView>(
        action_item_, &icon_label_view_delegate_);
    action_view_controller_ = std::make_unique<views::ActionViewController>();
    action_view_controller_->CreateActionViewRelationship(
        page_action_view_.get(), action_item_->GetAsWeakPtr());
  }

  void TearDown() override {
    action_view_controller_.reset();
    page_action_view_.reset();
    action_item_ = nullptr;
    actions::ActionManager::Get().ResetActions();
  }

  std::unique_ptr<PageActionController> NewPageActionController() const {
    auto controller = std::make_unique<PageActionController>();
    controller->Initialize({action_item_->GetActionId().value()});
    return controller;
  }

  void RegisterActionViewRelationship() {
    action_view_controller_->CreateActionViewRelationship(
        page_action_view_.get(), action_item_->GetAsWeakPtr());
  }

  PageActionView* page_action_view() { return page_action_view_.get(); }
  actions::ActionItem* action_item() { return action_item_; }

 private:
  std::unique_ptr<PageActionView> page_action_view_;
  std::unique_ptr<views::ActionViewController> action_view_controller_;
  raw_ptr<actions::ActionItem> action_item_;

  MockIconLabelViewDelegate icon_label_view_delegate_;

  // Must exist in order to create PageActionView during the test.
  views::LayoutProvider layout_provider_;
};

// Tests that calling Show/Hide will show/hide the view.
TEST_F(PageActionViewTest, ViewRespondsToPageActionUpdates) {
  auto controller = NewPageActionController();
  actions::ActionItem* item = action_item();
  item->SetEnabled(true);
  item->SetVisible(true);

  PageActionView* view = page_action_view();
  view->OnNewActiveController(controller.get());

  EXPECT_FALSE(view->GetVisible());
  controller->Show(0);
  EXPECT_TRUE(view->GetVisible());

  controller->Show(0);
  EXPECT_TRUE(view->GetVisible());

  controller->Hide(0);
  EXPECT_FALSE(view->GetVisible());

  // Disabling the ActionItem should prevent the view from showing.
  item->SetEnabled(false);
  controller->Show(0);
  EXPECT_FALSE(view->GetVisible());
  item->SetEnabled(true);

  item->SetVisible(false);
  controller->Show(0);
  EXPECT_FALSE(view->GetVisible());
}

// Tests that updating the ActionItem will update the view.
TEST_F(PageActionViewTest, ViewRespondsToActionItemUpdates) {
  auto controller = NewPageActionController();
  actions::ActionItem* item = action_item();
  PageActionView* view = page_action_view();

  item->SetVisible(true);
  item->SetEnabled(true);
  EXPECT_FALSE(view->GetVisible());  // No active controller yet.

  view->OnNewActiveController(controller.get());
  controller->Show(0);
  EXPECT_TRUE(view->GetVisible());

  item->SetVisible(false);
  EXPECT_FALSE(view->GetVisible());
  item->SetVisible(true);
  EXPECT_TRUE(view->GetVisible());

  item->SetEnabled(false);
  EXPECT_FALSE(view->GetVisible());
  item->SetEnabled(true);
  EXPECT_TRUE(view->GetVisible());
}

// Tests that calling Show/Hide on an inactive controller will not affect the
// view.
TEST_F(PageActionViewTest, ViewIgnoresInactiveController) {
  auto controller_a = NewPageActionController();
  auto controller_b = NewPageActionController();
  actions::ActionItem* item = action_item();
  item->SetEnabled(true);
  item->SetVisible(true);
  PageActionView* view = page_action_view();
  view->OnNewActiveController(controller_a.get());

  controller_a->Show(0);
  EXPECT_TRUE(view->GetVisible());

  controller_b->Hide(0);
  EXPECT_TRUE(view->GetVisible());

  controller_a->Hide(0);
  EXPECT_FALSE(view->GetVisible());

  controller_b->Show(0);
  EXPECT_FALSE(view->GetVisible());

  // Updating the active controller should apply the new model's state.
  view->OnNewActiveController(controller_b.get());
  EXPECT_TRUE(view->GetVisible());
}

// Tests that the PageActionView should never display when it doesn't have an
// active PageActionController.
TEST_F(PageActionViewTest, NoActiveController) {
  actions::ActionItem* item = action_item();
  item->SetEnabled(true);
  item->SetVisible(true);
  PageActionView* view = page_action_view();
  EXPECT_FALSE(view->GetVisible());

  auto controller = NewPageActionController();
  view->OnNewActiveController(controller.get());
  controller->Show(0);
  EXPECT_TRUE(view->GetVisible());

  view->OnNewActiveController(nullptr);
  EXPECT_FALSE(view->GetVisible());
}

}  // namespace
}  // namespace page_actions
