// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include <memory>

#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
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

class PageActionViewTest : public ChromeViewsTestBase {
 public:
  PageActionViewTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    // Use any arbitrary vector icon.
    auto image = ui::ImageModel::FromVectorIcon(vector_icons::kBackArrowIcon,
                                                ui::kColorSysPrimary,
                                                /*icon_size=*/16);
    action_item_ = actions::ActionManager::Get().AddAction(
        actions::ActionItem::Builder().SetActionId(0).SetImage(image).Build());
    page_action_view_ = std::make_unique<PageActionView>(
        action_item_, &icon_label_view_delegate_);
    action_view_controller_ = std::make_unique<views::ActionViewController>();
    action_view_controller_->CreateActionViewRelationship(
        page_action_view_.get(), action_item_->GetAsWeakPtr());

    profile_ = std::make_unique<TestingProfile>();
    pinned_actions_model_ =
        std::make_unique<PinnedToolbarActionsModel>(profile_.get());
  }

  void TearDown() override {
    ChromeViewsTestBase::TearDown();
    action_view_controller_.reset();
    page_action_view_.reset();
    action_item_ = nullptr;
    actions::ActionManager::Get().ResetActions();
    pinned_actions_model_.reset();
    profile_.reset();
  }

  std::unique_ptr<PageActionController> NewPageActionController() const {
    auto controller =
        std::make_unique<PageActionController>(pinned_actions_model_.get());
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

  std::unique_ptr<PinnedToolbarActionsModel> pinned_actions_model_;
  std::unique_ptr<TestingProfile> profile_;

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

// Test that OnThemeChanged updates the icon image correctly.
TEST_F(PageActionViewTest, OnThemeChangedUpdatesIconImage) {
  // Simulate OnThemeChanged.
  page_action_view()->OnThemeChanged();

  // Verify that UpdateIconImage is invoked and sets a valid image model.
  gfx::ImageSkia image_model =
      page_action_view()->GetImage(views::Button::STATE_NORMAL);
  ASSERT_FALSE(image_model.isNull());
}

// Test that UpdateBorder adjusts the insets based on label visibility.
TEST_F(PageActionViewTest, UpdateBorderAdjustsInsets) {
  // Test case: Label visibility is true.
  page_action_view()->SetShouldShowLabelForTesting(true);
  gfx::Insets initial_insets = page_action_view()->GetInsets();

  // Simulate UpdateBorder when label is visible.
  page_action_view()->UpdateBorder();
  gfx::Insets updated_insets_true = page_action_view()->GetInsets();

  // Verify that insets are updated when the label is visible.
  EXPECT_NE(initial_insets, updated_insets_true);

  // Test case: Label visibility is false.
  page_action_view()->SetShouldShowLabelForTesting(false);

  // Simulate UpdateBorder when label is not visible.
  page_action_view()->UpdateBorder();
  gfx::Insets updated_insets_false = page_action_view()->GetInsets();

  // Verify that insets remain unchanged when the label is not visible.
  EXPECT_EQ(initial_insets, updated_insets_false);

  // Verify that true and false cases result in different insets.
  EXPECT_NE(updated_insets_true, updated_insets_false);
}

}  // namespace
}  // namespace page_actions
