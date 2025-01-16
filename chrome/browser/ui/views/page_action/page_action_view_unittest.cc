// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/actions/actions.h"
#include "ui/events/test/test_event.h"
#include "ui/views/actions/action_view_controller.h"

namespace page_actions {
namespace {

using ::testing::Return;
using ::testing::ReturnRef;

constexpr int kDefaultIconSize = 16;

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

class MockPageActionModel : public PageActionModelInterface {
 public:
  MOCK_METHOD(bool, GetVisible, (), (const, override));
  MOCK_METHOD(const std::u16string, GetText, (), (const, override));
  MOCK_METHOD(const std::u16string, GetTooltipText, (), (const, override));
  MOCK_METHOD(const ui::ImageModel&, GetImage, (), (const, override));

  MOCK_METHOD(void,
              AddObserver,
              (PageActionModelObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (PageActionModelObserver * observer),
              (override));
  MOCK_METHOD(void,
              SetActionItemProperties,
              (base::PassKey<PageActionController>,
               const actions::ActionItem* action_item),
              (override));
  MOCK_METHOD(void,
              SetShowRequested,
              (base::PassKey<PageActionController>, bool requested),
              (override));
  MOCK_METHOD(void,
              SetOverrideText,
              (base::PassKey<PageActionController>,
               const std::optional<std::u16string>& text),
              (override));
};

// Test class that includes a real controller and model. Prefer to use simpler
// PageActionViewWithMockModelTest where possible.
// TODO(crbug.com/388527536): Move any tests possible to the mock model setup.
class PageActionViewTest : public ChromeViewsTestBase {
 public:
  PageActionViewTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    // Use any arbitrary vector icon.
    auto image = ui::ImageModel::FromVectorIcon(
        vector_icons::kBackArrowIcon, ui::kColorSysPrimary, kDefaultIconSize);
    action_item_ = actions::ActionManager::Get().AddAction(
        actions::ActionItem::Builder().SetActionId(0).SetImage(image).Build());
    page_action_view_ = std::make_unique<PageActionView>(
        action_item_, &icon_label_view_delegate_);
    profile_ = std::make_unique<TestingProfile>();
    pinned_actions_model_ =
        std::make_unique<PinnedToolbarActionsModel>(profile_.get());
  }

  void TearDown() override {
    ChromeViewsTestBase::TearDown();
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

  int GetViewImageWidth() {
    return page_action_view()
        ->GetImageModel(views::Button::STATE_NORMAL)
        ->Size()
        .width();
  }

  PageActionView* page_action_view() { return page_action_view_.get(); }
  actions::ActionItem* action_item() { return action_item_; }

 private:
  std::unique_ptr<PageActionView> page_action_view_;
  raw_ptr<actions::ActionItem> action_item_;

  testing::NiceMock<MockIconLabelViewDelegate> icon_label_view_delegate_;

  std::unique_ptr<PinnedToolbarActionsModel> pinned_actions_model_;
  std::unique_ptr<TestingProfile> profile_;

  // Must exist in order to create PageActionView during the test.
  views::LayoutProvider layout_provider_;
};

// Test class that uses a mock PageActionModel.
class PageActionViewWithMockModelTest : public ChromeViewsTestBase {
 public:
  PageActionViewWithMockModelTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    action_item_ = actions::ActionItem::Builder().SetActionId(0).Build();
    page_action_view_ = std::make_unique<PageActionView>(
        action_item_.get(), &icon_label_view_delegate_);
    page_action_view_->SetModel(model());

    ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));
    ON_CALL(mock_model_, GetText()).WillByDefault(Return(mock_string_));
    ON_CALL(mock_model_, GetTooltipText()).WillByDefault(Return(mock_string_));
    ON_CALL(mock_model_, GetImage()).WillByDefault(ReturnRef(mock_image_));
  }

  void TearDown() override {
    page_action_view_.reset();
    ChromeViewsTestBase::TearDown();
  }

  PageActionView* page_action_view() { return page_action_view_.get(); }
  MockPageActionModel* model() { return &mock_model_; }

 private:
  std::unique_ptr<actions::ActionItem> action_item_;
  std::unique_ptr<PageActionView> page_action_view_;
  testing::NiceMock<MockIconLabelViewDelegate> icon_label_view_delegate_;

  // Must exist in order to create PageActionView during the test.
  views::LayoutProvider layout_provider_;

  // Mock model and associated placeholder data.
  testing::NiceMock<MockPageActionModel> mock_model_;
  ui::ImageModel mock_image_;
  std::u16string mock_string_;
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

  // View defaults to invisible.
  EXPECT_FALSE(view->GetVisible());

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
  const int required_icon_size =
      GetLayoutConstant(LOCATION_BAR_TRAILING_ICON_SIZE);
  // If the default size is the intended icon size, this test is useless.
  EXPECT_GT(required_icon_size, kDefaultIconSize);

  auto controller = NewPageActionController();
  page_action_view()->OnNewActiveController(controller.get());
  action_item()->SetEnabled(true);
  action_item()->SetVisible(true);
  controller->Show(0);

  // When binding to a controller, model state is pushed into the view, so icon
  // should be correct size.
  EXPECT_EQ(GetViewImageWidth(), required_icon_size);

  // Icon resizes on theme change.
  page_action_view()->OnThemeChanged();
  EXPECT_EQ(GetViewImageWidth(), required_icon_size);
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

// Test that once a PageActionController is active, overriding the text updates
// the view, and that the override persists until the controller is removed.
TEST_F(PageActionViewTest, OverrideText) {
  const std::u16string kInitialText = u"Initial Text";
  actions::ActionItem* item = action_item();
  item->SetEnabled(true);
  item->SetVisible(true);
  item->SetText(kInitialText);

  PageActionView* view = page_action_view();
  EXPECT_FALSE(view->GetVisible());
  EXPECT_EQ(u"", view->GetText());

  auto controller = NewPageActionController();
  view->OnNewActiveController(controller.get());
  EXPECT_EQ(kInitialText, view->GetText());

  const std::u16string kOverrideText = u"Override Text";
  controller->Show(0);
  controller->OverrideText(0, kOverrideText);
  EXPECT_TRUE(view->GetVisible());
  EXPECT_EQ(kOverrideText, view->GetText());

  view->OnNewActiveController(nullptr);
  EXPECT_FALSE(view->GetVisible());
  EXPECT_EQ(kOverrideText, view->GetText());
}

class PageActionViewTriggerTest : public PageActionViewTest {
 public:
  PageActionViewTriggerTest() = default;
  ~PageActionViewTriggerTest() override = default;

  void SetUp() override {
    PageActionViewTest::SetUp();
    action_item()->SetInvokeActionCallback(base::BindRepeating(
        &PageActionViewTriggerTest::ActionInvocationCallback,
        base::Unretained(this)));
  }

  void ActionInvocationCallback(actions::ActionItem* item,
                                actions::ActionInvocationContext context) {
    const PageActionTrigger trigger = static_cast<PageActionTrigger>(
        context.GetProperty(kPageActionTriggerKey));
    switch (trigger) {
      case PageActionTrigger::kMouse:
        ++mouse_trigger_count_;
        break;
      case PageActionTrigger::kKeyboard:
        ++key_trigger_count_;
        break;
      case PageActionTrigger::kGesture:
        ++gesture_trigger_count_;
        break;
    }
  }

  int TotalTriggerCount() const {
    return mouse_trigger_count_ + key_trigger_count_ + gesture_trigger_count_;
  }
  int mouse_trigger_count() const { return mouse_trigger_count_; }
  int key_trigger_count() const { return key_trigger_count_; }
  int gesture_trigger_count() const { return gesture_trigger_count_; }

 private:
  int mouse_trigger_count_ = 0;
  int key_trigger_count_ = 0;
  int gesture_trigger_count_ = 0;
};

TEST_F(PageActionViewTriggerTest, PageActionKeyTriggerPropagation) {
  page_action_view()->NotifyClick(
      ui::test::TestEvent(ui::EventType::kKeyPressed));
  EXPECT_EQ(1, key_trigger_count());
  EXPECT_EQ(1, TotalTriggerCount());
}

TEST_F(PageActionViewTriggerTest, PageActionMouseTriggerPropagation) {
  page_action_view()->NotifyClick(
      ui::test::TestEvent(ui::EventType::kMousePressed));
  EXPECT_EQ(1, mouse_trigger_count());
  EXPECT_EQ(1, TotalTriggerCount());
}

TEST_F(PageActionViewTriggerTest, PageActionGestureTriggerPropagation) {
  page_action_view()->NotifyClick(
      ui::test::TestEvent(ui::EventType::kGestureTap));
  EXPECT_EQ(1, gesture_trigger_count());
  EXPECT_EQ(1, TotalTriggerCount());
}

TEST_F(PageActionViewWithMockModelTest, Visibility) {
  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(model());
  EXPECT_FALSE(page_action_view()->GetVisible());

  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(true));
  page_action_view()->OnPageActionModelChanged(model());
  EXPECT_TRUE(page_action_view()->GetVisible());
}

TEST_F(PageActionViewWithMockModelTest, SuggestionText) {
  const std::u16string kTestText = u"Test text";
  EXPECT_CALL(*model(), GetText()).WillRepeatedly(Return(kTestText));
  page_action_view()->OnPageActionModelChanged(model());
  EXPECT_EQ(page_action_view()->GetText(), kTestText);
}

}  // namespace
}  // namespace page_actions
