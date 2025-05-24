// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "chrome/browser/ui/views/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_model.h"
#include "chrome/browser/ui/views/page_action/test_support/test_page_action_properties_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/background.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/view_class_properties.h"

namespace page_actions {
namespace {

using ::testing::Return;
using ::testing::ReturnRef;
using ::ui::EventType;

constexpr int kDefaultIconSize = 16;
const std::u16string kTestText = u"Test text";

static constexpr actions::ActionId kTestPageActionId = 0;
static const PageActionPropertiesMap kTestProperties = PageActionPropertiesMap{
    {
        kTestPageActionId,
        PageActionProperties{
            .histogram_name = "Test",
            .is_ephemeral = true,
        },
    },
};

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

class AlwaysActiveTabInterface : public FakeTabInterface {
 public:
  explicit AlwaysActiveTabInterface(TestingProfile* profile)
      : FakeTabInterface(profile) {}

  ~AlwaysActiveTabInterface() override = default;
  bool IsActivated() const override { return true; }
};

// Test class that includes a real controller and model. Prefer to use simpler
// PageActionViewTest where possible.
class PageActionViewWithControllerTest : public ChromeViewsTestBase {
 public:
  PageActionViewWithControllerTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    // Use any arbitrary vector icon.
    auto image = ui::ImageModel::FromVectorIcon(
        vector_icons::kBackArrowIcon, ui::kColorSysPrimary, kDefaultIconSize);
    action_item_ = actions::ActionManager::Get().AddAction(
        actions::ActionItem::Builder()
            .SetActionId(kTestPageActionId)
            .SetImage(image)
            .Build());
    test_page_action_view_ = std::make_unique<PageActionView>(
        action_item_,
        PageActionViewParams{
            .icon_size = kDefaultIconSize,
            .icon_label_bubble_delegate = &icon_label_view_delegate_,
        },
        ui::ElementIdentifier());

    pinned_actions_model_ =
        std::make_unique<PinnedToolbarActionsModel>(&profile_);
  }

  void TearDown() override {
    ChromeViewsTestBase::TearDown();
    action_item_ = nullptr;
    actions::ActionManager::Get().ResetActions();
    pinned_actions_model_.reset();
  }

  std::unique_ptr<PageActionController> NewPageActionController(
      tabs::TabInterface& tab) const {
    auto controller = std::make_unique<PageActionController>(
        pinned_actions_model_.get());
    controller->Initialize(tab, {action_item_->GetActionId().value()},
                           TestPageActionPropertiesProvider(kTestProperties));
    return controller;
  }

  PageActionView* page_action_view() { return test_page_action_view_.get(); }
  actions::ActionItem* action_item() { return action_item_; }

 protected:
  TestingProfile profile_;

 private:
  std::unique_ptr<PageActionView> test_page_action_view_;
  raw_ptr<actions::ActionItem> action_item_;

  testing::NiceMock<MockIconLabelViewDelegate> icon_label_view_delegate_;

  std::unique_ptr<PinnedToolbarActionsModel> pinned_actions_model_;

  // Must exist in order to create PageActionView during the test.
  views::LayoutProvider layout_provider_;
};

// Base test class for PageActionView.  Uses a mock PageActionModel.
class PageActionViewTest : public ChromeViewsTestBase {
 public:
  PageActionViewTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    action_item_ =
        actions::ActionItem::Builder().SetActionId(kTestPageActionId).Build();

    // Host the view in a Widget so it can handle things like mouse input.
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();

    page_action_view_ =
        widget_->SetContentsView(std::make_unique<PageActionView>(
            action_item_.get(),
            PageActionViewParams{
                .icon_size = view_icon_size_,
                .icon_label_bubble_delegate = &icon_label_view_delegate_},
            ui::ElementIdentifier()));

    page_action_view_->GetSlideAnimationForTesting().SetSlideDuration(
        base::Seconds(0));

    ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));
    ON_CALL(mock_model_, GetShowSuggestionChip()).WillByDefault(Return(false));
    ON_CALL(mock_model_, GetShouldAnimateChip()).WillByDefault(Return(false));
    ON_CALL(mock_model_, GetText()).WillByDefault(ReturnRef(mock_string_));
    ON_CALL(mock_model_, GetAccessibleName())
        .WillByDefault(ReturnRef(mock_string_));
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

  PageActionView* page_action_view() { return page_action_view_.get(); }
  MockPageActionModel* model() { return &mock_model_; }
  actions::ActionItem* action_item() { return action_item_.get(); }
  int view_icon_size() const { return view_icon_size_; }

 protected:
  testing::NiceMock<MockIconLabelViewDelegate> icon_label_view_delegate_;

 private:
  std::unique_ptr<actions::ActionItem> action_item_;

  std::unique_ptr<views::Widget> widget_;

  // Owned by widget_.
  raw_ptr<PageActionView> page_action_view_;

  // Must exist in order to create PageActionView during the test.
  views::LayoutProvider layout_provider_;

  // Mock model and associated placeholder data.
  testing::NiceMock<MockPageActionModel> mock_model_;
  const ui::ImageModel mock_image_ =
      ui::ImageModel::FromVectorIcon(vector_icons::kBackArrowIcon,
                                     ui::kColorSysPrimary,
                                     kDefaultIconSize);
  std::u16string mock_string_ = kTestText;

  const int view_icon_size_ = kDefaultIconSize;
};

TEST_F(PageActionViewTest, ViewHasCorrectElementIdentifier) {
  const ui::ElementIdentifier kCustomIdentifier =
      ui::ElementIdentifier::FromName("PageActionViewTestIdentifier");

  auto view_with_id = std::make_unique<PageActionView>(
      action_item(),
      PageActionViewParams{
          .icon_size = view_icon_size(),
          .icon_label_bubble_delegate = &icon_label_view_delegate_},
      kCustomIdentifier);

  EXPECT_EQ(view_with_id->GetProperty(views::kElementIdentifierKey),
            kCustomIdentifier);
}

// Tests that calling Show/Hide on an inactive controller will not affect the
// view.
TEST_F(PageActionViewWithControllerTest, ViewIgnoresInactiveController) {
  // Use an always-active tab to ensure consistent visibility updates.
  AlwaysActiveTabInterface tab(&profile_);
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
TEST_F(PageActionViewWithControllerTest, NoActiveController) {
  actions::ActionItem* item = action_item();
  item->SetEnabled(true);
  item->SetVisible(true);
  PageActionView* view = page_action_view();
  EXPECT_FALSE(view->GetVisible());

  // Use an always-active tab to ensure consistent visibility updates.
  AlwaysActiveTabInterface tab(&profile_);
  auto controller = NewPageActionController(tab);
  view->OnNewActiveController(controller.get());
  controller->Show(0);
  EXPECT_TRUE(view->GetVisible());

  view->OnNewActiveController(nullptr);
  EXPECT_FALSE(view->GetVisible());
}

TEST_F(PageActionViewTest, Visibility) {
  // Ensure view defaults to invisible.
  EXPECT_FALSE(page_action_view()->GetVisible());

  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(true));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_TRUE(page_action_view()->GetVisible());

  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_FALSE(page_action_view()->GetVisible());
}

TEST_F(PageActionViewTest, LabelVisibility) {
  // Ensure view defaults to invisible.
  EXPECT_FALSE(page_action_view()->GetVisible());

  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetText()).WillRepeatedly(ReturnRef(kTestText));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_TRUE(page_action_view()->GetVisible());
  EXPECT_TRUE(page_action_view()->IsChipVisible());

  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_TRUE(page_action_view()->GetVisible());
  EXPECT_FALSE(page_action_view()->IsChipVisible());
}

TEST_F(PageActionViewTest, ChipStateUpdatesBackgroundColor) {
  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetText()).WillRepeatedly(ReturnRef(kTestText));
  page_action_view()->OnPageActionModelChanged(*model());

  ASSERT_NE(page_action_view()->GetBackground(), nullptr);
  EXPECT_EQ(page_action_view()->GetBackground()->color(),
            page_action_view()->GetColorProvider()->GetColor(
                kColorOmniboxIconBackgroundTonal));

  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());

  EXPECT_EQ(page_action_view()->GetBackground(), nullptr);
}

TEST_F(PageActionViewTest, ChipStateUpdatesForegroundColor) {
  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetText()).WillRepeatedly(ReturnRef(kTestText));
  page_action_view()->OnPageActionModelChanged(*model());

  ASSERT_TRUE(page_action_view()->GetVisible());
  ASSERT_TRUE(page_action_view()->IsChipVisible());

  const SkColor expected_color =
      page_action_view()->GetColorProvider()->GetColor(
          kColorOmniboxIconForegroundTonal);
  EXPECT_EQ(page_action_view()->GetCurrentTextColor(), expected_color);
}

TEST_F(PageActionViewTest, SuggestionText) {
  EXPECT_CALL(*model(), GetText()).WillRepeatedly(ReturnRef(kTestText));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_EQ(page_action_view()->GetText(), kTestText);
}

TEST_F(PageActionViewTest, TooltipText) {
  EXPECT_CALL(*model(), GetTooltipText()).WillRepeatedly(ReturnRef(kTestText));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_EQ(page_action_view()->GetTooltipText(), kTestText);
}

// Test that OnThemeChanged updates the icon image correctly.
TEST_F(PageActionViewTest, OnThemeChangedUpdatesIconImage) {
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

// Test that UpdateIconImage() correctly handles ImageModels created without a
// vector icon
TEST_F(PageActionViewTest, UpdateIconImageHandlesDifferentImageTypes) {
  // Set up for a non vector icon.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kDefaultIconSize, kDefaultIconSize);
  const ui::ImageModel bitmap_image =
      ui::ImageModel::FromImage(gfx::Image::CreateFrom1xBitmap(bitmap));
  EXPECT_CALL(*model(), GetImage()).WillRepeatedly(ReturnRef(bitmap_image));

  // Trigger the icon update.
  page_action_view()->OnPageActionModelChanged(*model());

  // Check that the image model in the PageActionView is correctly set and is
  // not a vector icon.
  EXPECT_FALSE(page_action_view()
                   ->GetImageModel(views::Button::STATE_NORMAL)
                   ->IsEmpty());
  EXPECT_FALSE(page_action_view()
                   ->GetImageModel(views::Button::STATE_NORMAL)
                   ->IsVectorIcon());
}

// TODO(crbug.com/411078148): Re-enable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ChipAnnouncements DISABLED_ChipAnnouncements
#else
#define MAYBE_ChipAnnouncements ChipAnnouncements
#endif
TEST_F(PageActionViewTest, MAYBE_ChipAnnouncements) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  ASSERT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  // Initialize the page action so that the chip is showing, but announcements
  // are disabled.
  EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model(), GetShouldAnnounceChip()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  // Enabling the announcements now shouldn't trigger anything, since the chip
  // is already showing.
  EXPECT_CALL(*model(), GetShouldAnnounceChip()).WillRepeatedly(Return(true));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  // Hide the suggestion chip, then re-show it. This should trigger an
  // announcement.
  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  page_action_view()->OnPageActionModelChanged(*model());
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
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

TEST_F(PageActionViewTriggerTest, PageActionTriggersOnKeyboardClick) {
  EXPECT_CALL(*model(), GetActionItemIsShowingBubble())
      .WillRepeatedly(Return(false));
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      page_action_view(), ui::test::InteractionTestUtil::InputType::kKeyboard);
  EXPECT_EQ(1, TotalTriggerCount());
}

TEST_F(PageActionViewTriggerTest, PageActionTriggersOnMouseClick) {
  EXPECT_CALL(*model(), GetActionItemIsShowingBubble())
      .WillRepeatedly(Return(false));
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      page_action_view(), ui::test::InteractionTestUtil::InputType::kMouse);
  EXPECT_EQ(1, TotalTriggerCount());
}

TEST_F(PageActionViewTriggerTest, PageActionMouseRightClickIgnored) {
  ui::MouseEvent mouse_press(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(),
                             ui::EF_RIGHT_MOUSE_BUTTON,
                             ui::EF_RIGHT_MOUSE_BUTTON);
  ui::MouseEvent mouse_release(ui::EventType::kMouseReleased, gfx::Point(),
                               gfx::Point(), ui::EventTimeForNow(),
                               ui::EF_RIGHT_MOUSE_BUTTON,
                               ui::EF_RIGHT_MOUSE_BUTTON);
  page_action_view()->OnMouseEvent(&mouse_press);
  page_action_view()->OnMouseEvent(&mouse_release);
  EXPECT_EQ(0, TotalTriggerCount());
}

// Action invocations are suppressed when the ActionItem is displaying UI.
TEST_F(PageActionViewTriggerTest, PageActionDoesNotTriggerIfBubbleShowing) {
  EXPECT_CALL(*model(), GetActionItemIsShowingBubble())
      .WillRepeatedly(Return(true));
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      page_action_view(), ui::test::InteractionTestUtil::InputType::kMouse);
  EXPECT_EQ(0, TotalTriggerCount());
}

class PageActionViewAnimationTest : public PageActionViewTest {
 public:
  using PageActionViewTest::PageActionViewTest;

  void SetUp() override {
    PageActionViewTest::SetUp();
    animation_ = std::make_unique<gfx::AnimationTestApi>(
        &page_action_view()->GetSlideAnimationForTesting());
  }

  void TearDown() override {
    animation_.reset();
    PageActionViewTest::TearDown();
  }

  void SetInitialChipVisibility(bool showing) {
    // Make the visibility change instant.
    page_action_view()->GetSlideAnimationForTesting().SetSlideDuration(
        base::Seconds(0));
    EXPECT_CALL(*model(), GetShowSuggestionChip())
        .WillRepeatedly(Return(showing));
    EXPECT_CALL(*model(), GetVisible()).WillRepeatedly(Return(true));
    EXPECT_CALL(*model(), GetText()).WillRepeatedly(ReturnRef(kTestText));
    EXPECT_CALL(*model(), GetAccessibleName())
        .WillRepeatedly(ReturnRef(kTestText));
    page_action_view()->OnPageActionModelChanged(*model());

    ASSERT_FALSE(page_action_view()->is_animating_label());
    ASSERT_EQ(page_action_view()->IsChipVisible(), showing);
  }

  // Force the animation to extend beyond the duration of this test, allowing
  // us to inspect the view's state mid-animation.
  void ExtendAnimations() {
    page_action_view()->GetSlideAnimationForTesting().SetSlideDuration(
        extended_animation_duration_);
  }

  // Force the current animation to given percentage.
  void FastForwardAnimation(double progress = 1.0) {
    auto now = base::TimeTicks::Now();
    animation_->SetStartTime(now);
    animation_->Step(now + (progress * extended_animation_duration_));
  }

  gfx::AnimationTestApi& animation() { return *animation_.get(); }

 private:
  std::unique_ptr<gfx::AnimationTestApi> animation_;

  const base::TimeDelta extended_animation_duration_ = base::Hours(1);
};

TEST_F(PageActionViewAnimationTest, ChipStateDuringAnimateOut) {
  EXPECT_CALL(*model(), GetShouldAnimateChip()).WillRepeatedly(Return(true));
  SetInitialChipVisibility(true);
  ExtendAnimations();

  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());

  // The page action should be in the middle of animating and its chip should
  // be visible.
  EXPECT_TRUE(page_action_view()->is_animating_label());
  EXPECT_TRUE(page_action_view()->IsChipVisible());
  EXPECT_NE(page_action_view()->GetBackground(), nullptr);

  // Skip the animation to its ending.
  FastForwardAnimation();

  // The page action should no longer be animating and its chip should be
  // hidden.
  EXPECT_FALSE(page_action_view()->is_animating_label());
  EXPECT_FALSE(page_action_view()->IsChipVisible());
  EXPECT_EQ(page_action_view()->GetBackground(), nullptr);
}

TEST_F(PageActionViewAnimationTest, ChipStateDuringAnimateIn) {
  EXPECT_CALL(*model(), GetShouldAnimateChip()).WillRepeatedly(Return(true));
  SetInitialChipVisibility(false);
  ExtendAnimations();

  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  page_action_view()->OnPageActionModelChanged(*model());

  // The page action should be in the middle of animating and its chip should
  // be visible.
  EXPECT_TRUE(page_action_view()->is_animating_label());
  EXPECT_TRUE(page_action_view()->IsChipVisible());
  EXPECT_NE(page_action_view()->GetBackground(), nullptr);

  // Skip the animation to its ending.
  FastForwardAnimation();

  // The page action should no longer be animating and its chip should be
  // visible.
  EXPECT_FALSE(page_action_view()->is_animating_label());
  EXPECT_TRUE(page_action_view()->IsChipVisible());
  EXPECT_NE(page_action_view()->GetBackground(), nullptr);
}

TEST_F(PageActionViewAnimationTest, AnimationsDisabled) {
  gfx::Animation::SetPrefersReducedMotionForTesting(false);
  ASSERT_FALSE(gfx::Animation::PrefersReducedMotion());
  SetInitialChipVisibility(false);

  ExtendAnimations();
  EXPECT_CALL(*model(), GetShouldAnimateChip()).WillRepeatedly(Return(false));
  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(true));
  page_action_view()->OnPageActionModelChanged(*model());

  EXPECT_FALSE(page_action_view()->is_animating_label());
  EXPECT_TRUE(page_action_view()->IsChipVisible());

  EXPECT_CALL(*model(), GetShowSuggestionChip()).WillRepeatedly(Return(false));
  page_action_view()->OnPageActionModelChanged(*model());

  EXPECT_FALSE(page_action_view()->is_animating_label());
  EXPECT_FALSE(page_action_view()->IsChipVisible());
}

}  // namespace
}  // namespace page_actions
