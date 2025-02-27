// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/mock_page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/test_event.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace page_actions {
namespace {

using ::testing::Return;
using ::testing::ReturnRef;
using ::ui::EventType;

constexpr int kDefaultIconSize = 16;
const std::u16string kTestText = u"Test text";

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

class AlwaysActiveTabInterface : public tabs::MockTabInterface {
 public:
  ~AlwaysActiveTabInterface() override = default;
  bool IsActivated() const override { return true; }
};

// Some methods in IconLabelBubbleView, from which PageActionView inherits,
// do not provide getters for certain properties.
// This class wraps PageActionView to monitor calls to the view for those
// properties that cannot be retrieved via a getter.
class TestPageActionView : public PageActionView {
 public:
  // Inherit parent constructors.
  using PageActionView::PageActionView;

  void SetUseTonalColorsWhenExpanded(bool use_tonal_colors) final {
    use_tonal_colors_ = use_tonal_colors;
    PageActionView::SetUseTonalColorsWhenExpanded(use_tonal_colors);
  }

  void SetBackgroundVisibility(
      BackgroundVisibility background_visibility) final {
    background_visibility_ = background_visibility;
    PageActionView::SetBackgroundVisibility(background_visibility);
  }

  bool is_using_tonal_colors() const { return use_tonal_colors_; }
  BackgroundVisibility background_visible() const {
    return background_visibility_;
  }

 private:
  bool use_tonal_colors_ = false;
  BackgroundVisibility background_visibility_ = BackgroundVisibility::kNever;
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
    test_page_action_view_ = std::make_unique<TestPageActionView>(
        action_item_,
        PageActionViewParams{
            .icon_size = kDefaultIconSize,
            .icon_label_bubble_delegate = &icon_label_view_delegate_,
        },
        /*chip_state_changed_callback=*/base::DoNothing());

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

  std::unique_ptr<PageActionController> NewPageActionController(
      tabs::TabInterface& tab) const {
    auto controller =
        std::make_unique<PageActionController>(pinned_actions_model_.get());
    controller->Initialize(tab, {action_item_->GetActionId().value()});
    return controller;
  }

  TestPageActionView* page_action_view() {
    return test_page_action_view_.get();
  }
  actions::ActionItem* action_item() { return action_item_; }

 private:
  std::unique_ptr<PageActionView> page_action_view_;
  std::unique_ptr<TestPageActionView> test_page_action_view_;
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

    // Host the view in a Widget so it can handle things like mouse input.
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();

    page_action_view_ =
        widget_->SetContentsView(std::make_unique<TestPageActionView>(
            action_item_.get(),
            PageActionViewParams{
                .icon_size = view_icon_size_,
                .icon_label_bubble_delegate = &icon_label_view_delegate_},
            /*chip_state_changed_callback=*/base::DoNothing()));

    ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));
    ON_CALL(mock_model_, GetShowSuggestionChip()).WillByDefault(Return(false));
    ON_CALL(mock_model_, GetText()).WillByDefault(ReturnRef(mock_string_));
    ON_CALL(mock_model_, GetTooltipText())
        .WillByDefault(ReturnRef(mock_string_));
    ON_CALL(mock_model_, GetImage()).WillByDefault(ReturnRef(mock_image_));

    page_action_view_->SetModel(model());
  }

  void TearDown() override {
    page_action_view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestPageActionView* page_action_view() { return page_action_view_.get(); }
  MockPageActionModel* model() { return &mock_model_; }
  actions::ActionItem* action_item() { return action_item_.get(); }
  int view_icon_size() const { return view_icon_size_; }

 private:
  std::unique_ptr<actions::ActionItem> action_item_;

  std::unique_ptr<views::Widget> widget_;

  // Owned by widget_.
  raw_ptr<TestPageActionView> page_action_view_;

  testing::NiceMock<MockIconLabelViewDelegate> icon_label_view_delegate_;

  // Must exist in order to create PageActionView during the test.
  views::LayoutProvider layout_provider_;

  // Mock model and associated placeholder data.
  testing::NiceMock<MockPageActionModel> mock_model_;
  ui::ImageModel mock_image_;
  std::u16string mock_string_;

  const int view_icon_size_ = kDefaultIconSize;
};

// Tests that calling Show/Hide on an inactive controller will not affect the
// view.
TEST_F(PageActionViewTest, ViewIgnoresInactiveController) {
  // Use an always-active tab to ensure consistent visibility updates.
  AlwaysActiveTabInterface tab;
  auto controller_a = NewPageActionController(tab);
  auto controller_b = NewPageActionController(tab);
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

  // Use an always-active tab to ensure consistent visibility updates.
  AlwaysActiveTabInterface tab;
  auto controller = NewPageActionController(tab);
  view->OnNewActiveController(controller.get());
  controller->Show(0);
  EXPECT_TRUE(view->GetVisible());

  view->OnNewActiveController(nullptr);
  EXPECT_FALSE(view->GetVisible());
}

TEST_F(PageActionViewWithMockModelTest, Visibility) {
  // Ensure view defaults to invisible.
  EXPECT_FALSE(page_action_view()->GetVisible());

  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(true));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_TRUE(page_action_view()->GetVisible());

  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_FALSE(page_action_view()->GetVisible());
}

TEST_F(PageActionViewWithMockModelTest, LabelVisibility) {
  // Ensure view defaults to invisible.
  EXPECT_FALSE(page_action_view()->GetVisible());

  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetText()).WillRepeatedly(ReturnRef(kTestText));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_TRUE(page_action_view()->GetVisible());
  EXPECT_TRUE(page_action_view()->ShouldShowLabel());
  EXPECT_TRUE(page_action_view()->GetLabelForTesting()->GetVisible());

  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_TRUE(page_action_view()->GetVisible());
  EXPECT_FALSE(page_action_view()->ShouldShowLabel());
  EXPECT_FALSE(page_action_view()->GetLabelForTesting()->GetVisible());
}

TEST_F(PageActionViewWithMockModelTest,
       UpdateStyleSetsTonalColorsAndBackgroundVisibility) {
  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  page_action_view()->OnPageActionModelChanged(*model());

  EXPECT_TRUE(page_action_view()->is_using_tonal_colors());
  EXPECT_EQ(page_action_view()->background_visible(),
            IconLabelBubbleView::BackgroundVisibility::kAlways);

  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());

  EXPECT_FALSE(page_action_view()->is_using_tonal_colors());
  EXPECT_EQ(page_action_view()->background_visible(),
            IconLabelBubbleView::BackgroundVisibility::kNever);
}

TEST_F(PageActionViewWithMockModelTest, SuggestionText) {
  EXPECT_CALL(*model(), GetText()).WillRepeatedly(ReturnRef(kTestText));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_EQ(page_action_view()->GetText(), kTestText);
}

TEST_F(PageActionViewWithMockModelTest, TooltipText) {
  EXPECT_CALL(*model(), GetTooltipText()).WillRepeatedly(ReturnRef(kTestText));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_EQ(page_action_view()->GetTooltipText(), kTestText);
}

// Test that OnThemeChanged updates the icon image correctly.
TEST_F(PageActionViewWithMockModelTest, OnThemeChangedUpdatesIconImage) {
  // If the default size is the intended icon size, this test is useless.
  const int kOriginalIconSize = view_icon_size() + 1;
  auto icon_image = ui::ImageModel::FromVectorIcon(
      vector_icons::kBackArrowIcon, ui::kColorSysPrimary, kOriginalIconSize);
  EXPECT_CALL(*model(), GetImage()).WillRepeatedly(ReturnRef(icon_image));

  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_EQ(page_action_view()
                ->GetImageModel(views::Button::STATE_NORMAL)
                ->Size()
                .width(),
            view_icon_size());

  // Icon maintains required size on theme change.
  page_action_view()->OnThemeChanged();
  EXPECT_EQ(page_action_view()
                ->GetImageModel(views::Button::STATE_NORMAL)
                ->Size()
                .width(),
            view_icon_size());
}

// Test that UpdateBorder adjusts the insets based on label visibility.
TEST_F(PageActionViewWithMockModelTest, UpdateBorderAdjustsInsets) {
  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetText()).WillRepeatedly(ReturnRef(kTestText));
  page_action_view()->OnPageActionModelChanged(*model());
  const gfx::Insets initial_insets = page_action_view()->GetInsets();

  page_action_view()->UpdateBorder();
  const gfx::Insets insets_with_chip = page_action_view()->GetInsets();

  EXPECT_EQ(initial_insets, insets_with_chip);

  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());

  page_action_view()->UpdateBorder();
  const gfx::Insets insets_without_chip = page_action_view()->GetInsets();

  EXPECT_NE(initial_insets, insets_without_chip);
  EXPECT_NE(insets_with_chip, insets_without_chip);
}

class PageActionViewTriggerTest : public PageActionViewWithMockModelTest {
 public:
  PageActionViewTriggerTest() = default;
  ~PageActionViewTriggerTest() override = default;

  void SetUp() override {
    PageActionViewWithMockModelTest::SetUp();
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
  page_action_view()->NotifyClick(ui::test::TestEvent(EventType::kKeyPressed));
  EXPECT_EQ(1, key_trigger_count());
  EXPECT_EQ(1, TotalTriggerCount());
}

TEST_F(PageActionViewTriggerTest, PageActionMouseTriggerPropagation) {
  page_action_view()->NotifyClick(
      ui::test::TestEvent(EventType::kMousePressed));
  EXPECT_EQ(1, mouse_trigger_count());
  EXPECT_EQ(1, TotalTriggerCount());
}

TEST_F(PageActionViewTriggerTest, PageActionGestureTriggerPropagation) {
  page_action_view()->NotifyClick(ui::test::TestEvent(EventType::kGestureTap));
  EXPECT_EQ(1, gesture_trigger_count());
  EXPECT_EQ(1, TotalTriggerCount());
}

TEST_F(PageActionViewTriggerTest, PageActionTriggersOnMouseClick) {
  EXPECT_CALL(*model(), GetActionItemIsShowingBubble())
      .WillRepeatedly(Return(false));
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      page_action_view(), ui::test::InteractionTestUtil::InputType::kMouse);
  EXPECT_EQ(1, TotalTriggerCount());
}

// Action invocations are suppressed when the ActionItem is displaying UI.
TEST_F(PageActionViewTriggerTest, PageActionDoesNotTriggersIfBubbleShowing) {
  EXPECT_CALL(*model(), GetActionItemIsShowingBubble())
      .WillRepeatedly(Return(true));
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      page_action_view(), ui::test::InteractionTestUtil::InputType::kMouse);
  EXPECT_EQ(0, TotalTriggerCount());
}

// Action invocation suppression carries state across mouse events. Ensure that
// state is cleaned up, and isn't carried into a subsequent key event. The
// alternate way to test this a ForTest getter.
TEST_F(PageActionViewTriggerTest, PageActionsSuccessiveTriggers) {
  EXPECT_CALL(*model(), GetActionItemIsShowingBubble())
      .WillRepeatedly(Return(true));
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      page_action_view(), ui::test::InteractionTestUtil::InputType::kMouse);
  EXPECT_EQ(0, TotalTriggerCount());

  // A subsequent keyboard click should work.
  ui::KeyEvent key_event(EventType::kKeyPressed, ui::VKEY_RETURN, ui::EF_NONE);
  EXPECT_CALL(*model(), GetActionItemIsShowingBubble())
      .WillRepeatedly(Return(false));
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      page_action_view(), ui::test::InteractionTestUtil::InputType::kKeyboard);
  EXPECT_EQ(1, TotalTriggerCount());
}

}  // namespace
}  // namespace page_actions
