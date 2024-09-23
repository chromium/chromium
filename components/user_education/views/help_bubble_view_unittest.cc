// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_view.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "components/user_education/common/events.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace user_education {

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId);
const ui::ElementContext kTestElementContext{1};
constexpr gfx::Rect kWidgetBounds{400, 200, 200, 200};
}  // namespace

// Unit tests for HelpBubbleView. Timeout functionality isn't tested here due to
// the vagaries of trying to get simulated timed events to run without a full
// execution environment (specifically, Mac tests were extremely flaky without
// the browser).
//
// Timeouts are tested in:
// chrome/browser/ui/views/user_education/help_bubble_view_timeout_unittest.cc
class HelpBubbleViewTest : public views::ViewsTestBase {
 public:
  HelpBubbleViewTest() = default;
  ~HelpBubbleViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<test::TestThemedWidget>();
    widget_->Init(CreateParamsForTestWidget());
    view_ = widget_->SetContentsView(std::make_unique<views::View>());
    widget_->Show();
    widget_->SetBounds(kWidgetBounds);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  gfx::Rect GetWidgetClientBounds() const {
    return widget_->GetClientAreaBoundsInScreen();
  }

  HelpBubbleView* CreateHelpBubbleView(
      HelpBubbleParams params,
      std::optional<gfx::Rect> bounds = std::nullopt,
      std::optional<views::View*> view = std::nullopt) {
    internal::HelpBubbleAnchorParams anchor_params;
    anchor_params.view = view.value_or(view_);
    anchor_params.rect = bounds;
    return new HelpBubbleView(&test_delegate_, anchor_params,
                              std::move(params));
  }

  HelpBubbleView* CreateHelpBubbleView(base::RepeatingClosure button_callback) {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;

    if (button_callback) {
      HelpBubbleButtonParams button_params;
      button_params.text = u"Go away";
      button_params.is_default = true;
      button_params.callback = std::move(button_callback);
      params.buttons.push_back(std::move(button_params));
    }

    return CreateHelpBubbleView(std::move(params));
  }

  test::TestHelpBubbleDelegate test_delegate_;
  raw_ptr<views::View, DanglingUntriaged> view_;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(HelpBubbleViewTest, DefaultMaxWidth) {
  HelpBubbleParams params;

  // Choose body text that will wrap.
  params.body_text =
      u"The quick brown fox jumped over the lazy dogs. How now brown cow.";
  HelpBubbleButtonParams button1;
  button1.is_default = true;
  button1.text = u"button1";
  params.buttons.emplace_back(std::move(button1));
  HelpBubbleButtonParams button2;
  button2.is_default = false;
  button2.text = u"button2";
  params.buttons.emplace_back(std::move(button2));

  HelpBubbleView* const bubble = CreateHelpBubbleView(std::move(params));
  EXPECT_EQ(HelpBubbleView::kMaxWidthDip, bubble->GetPreferredSize().width());
  bubble->GetWidget()->Close();
}

TEST_F(HelpBubbleViewTest, ExpandedMaxWidth) {
  HelpBubbleParams params;

  // Choose body text that will wrap.
  params.body_text =
      u"The quick brown fox jumped over the lazy dogs. How now brown cow.";
  HelpBubbleButtonParams button1;
  button1.is_default = true;
  button1.text = u"Lorem ipsum dolor sit amet, consectetur adipiscing elit";
  params.buttons.emplace_back(std::move(button1));
  HelpBubbleButtonParams button2;
  button2.is_default = false;
  button2.text = u"button2";
  params.buttons.emplace_back(std::move(button2));

  HelpBubbleView* const bubble = CreateHelpBubbleView(std::move(params));
  EXPECT_GT(bubble->GetPreferredSize().width(), HelpBubbleView::kMaxWidthDip);
  bubble->GetWidget()->Close();
}

TEST_F(HelpBubbleViewTest, CallButtonCallback_Mouse) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, mock_callback);

  HelpBubbleView* const bubble = CreateHelpBubbleView(mock_callback.Get());

  // Simulate clicks on dismiss button.
  EXPECT_CALL_IN_SCOPE(
      mock_callback, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          bubble->GetDefaultButtonForTesting(),
          ui::test::InteractionTestUtil::InputType::kMouse));

  bubble->GetWidget()->Close();
}

TEST_F(HelpBubbleViewTest, CallButtonCallback_Keyboard) {
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, mock_callback);

  HelpBubbleView* const bubble = CreateHelpBubbleView(mock_callback.Get());

  // Simulate clicks on dismiss button.
  EXPECT_CALL_IN_SCOPE(
      mock_callback, Run,
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          bubble->GetDefaultButtonForTesting(),
          ui::test::InteractionTestUtil::InputType::kKeyboard));

  bubble->GetWidget()->Close();
}

TEST_F(HelpBubbleViewTest, StableButtonOrder) {
  HelpBubbleParams params;
  params.body_text = u"To X, do Y";
  params.arrow = HelpBubbleArrow::kTopRight;

  constexpr char16_t kButton1Text[] = u"button 1";
  constexpr char16_t kButton2Text[] = u"button 2";
  constexpr char16_t kButton3Text[] = u"button 3";

  HelpBubbleButtonParams button1;
  button1.text = kButton1Text;
  button1.is_default = false;
  params.buttons.push_back(std::move(button1));

  HelpBubbleButtonParams button2;
  button2.text = kButton2Text;
  button2.is_default = true;
  params.buttons.push_back(std::move(button2));

  HelpBubbleButtonParams button3;
  button3.text = kButton3Text;
  button3.is_default = false;
  params.buttons.push_back(std::move(button3));

  auto* bubble = new HelpBubbleView(
      &test_delegate_, internal::HelpBubbleAnchorParams{view_.get()},
      std::move(params));
  EXPECT_EQ(kButton1Text, bubble->GetNonDefaultButtonForTesting(0)->GetText());
  EXPECT_EQ(kButton2Text, bubble->GetDefaultButtonForTesting()->GetText());
  EXPECT_EQ(kButton3Text, bubble->GetNonDefaultButtonForTesting(1)->GetText());
}

TEST_F(HelpBubbleViewTest, AnchorToRect) {
  HelpBubbleParams params;
  params.body_text = u"To X, do Y";
  params.arrow = HelpBubbleArrow::kRightCenter;

  const auto widget_bounds = GetWidgetClientBounds();
  gfx::Rect anchor_bounds = widget_bounds;
  anchor_bounds.Inset(50);

  HelpBubbleView* const bubble =
      CreateHelpBubbleView(std::move(params), anchor_bounds);
  const auto bubble_bounds = bubble->GetWidget()->GetWindowBoundsInScreen();

  // The right side of the bubble should overlap the widget.
  EXPECT_TRUE(widget_bounds.Contains(bubble_bounds.right_center()));

  // The right side of the widget should be outside and aligned with the center
  // of the anchor bounds. Allow for rounding error when checking alignment.
  EXPECT_LT(bubble_bounds.right(), anchor_bounds.x());
  EXPECT_LE(std::abs(bubble_bounds.CenterPoint().y() -
                     anchor_bounds.CenterPoint().y()),
            2);
}

TEST_F(HelpBubbleViewTest, AnchorRectUpdated) {
  HelpBubbleParams params;
  params.body_text = u"To X, do Y";
  params.arrow = HelpBubbleArrow::kRightCenter;

  const auto widget_bounds = GetWidgetClientBounds();
  gfx::Rect anchor_bounds = widget_bounds;
  anchor_bounds.Inset(50);

  HelpBubbleView* const bubble =
      CreateHelpBubbleView(std::move(params), anchor_bounds);
  const auto bubble_bounds = bubble->GetWidget()->GetWindowBoundsInScreen();

  constexpr gfx::Vector2d kAnchorOffset{9, 13};
  anchor_bounds.Offset(kAnchorOffset);
  bubble->SetForceAnchorRect(anchor_bounds);
  bubble->OnAnchorBoundsChanged();

  gfx::Rect expected = bubble_bounds;
  expected.Offset(kAnchorOffset);
  EXPECT_EQ(expected, bubble->GetWidget()->GetWindowBoundsInScreen());
}

TEST_F(HelpBubbleViewTest, ScrollAnchorViewToVisible) {
  views::ScrollView* scroll_view = nullptr;
  views::View* anchor_view = nullptr;

  // Add an `anchor_view` to the `view_` hierarchy that is hosted within a
  // `scroll_view` and is initially outside the viewport.
  views::Builder<views::View>(view_)
      .SetUseDefaultFillLayout(true)
      .AddChildren(
          views::Builder<views::ScrollView>()
              .CopyAddressTo(&scroll_view)
              .ClipHeightTo(/*min_height=*/0, /*max_height=*/view_->height())
              .SetContents(
                  views::Builder<views::FlexLayoutView>()
                      .SetCrossAxisAlignment(views::LayoutAlignment::kEnd)
                      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
                      .SetOrientation(views::LayoutOrientation::kVertical)
                      .AddChildren(
                          views::Builder<views::View>().SetPreferredSize(
                              view_->size()),
                          views::Builder<views::View>()
                              .CopyAddressTo(&anchor_view)
                              .SetPreferredSize(gfx::Size(10, 10)))))
      .BuildChildren();

  // Ensure `widget_` has finished processing the layout changes.
  views::test::RunScheduledLayout(widget_.get());

  // Initially `anchor_view` should not be visible.
  EXPECT_FALSE(scroll_view->GetBoundsInScreen().Contains(
      anchor_view->GetBoundsInScreen()));

  // Expect that `scroll_view` will scroll when creating a help bubble anchored
  // to `anchor_view` since it is outside the viewport.
  base::MockRepeatingClosure callback;
  EXPECT_CALL(callback, Run()).Times(testing::AtLeast(1));
  base::CallbackListSubscription subscription =
      scroll_view->AddContentsScrolledCallback(callback.Get());

  // Create the help bubble anchored to `anchor_view`.
  HelpBubbleParams params;
  params.body_text = u"To X, do Y";
  params.arrow = HelpBubbleArrow::kTopRight;
  CreateHelpBubbleView(std::move(params), /*bounds=*/std::nullopt, anchor_view);

  // Expect that `anchor_view` is now visible.
  EXPECT_TRUE(scroll_view->GetBoundsInScreen().Contains(
      anchor_view->GetBoundsInScreen()));
}

class HelpBubbleViewsTest : public HelpBubbleViewTest {
 public:
  HelpBubbleViewsTest() = default;
  ~HelpBubbleViewsTest() override = default;

  // This simulates logic used by e.g. FloatingWebUIHelpBubbleFactory.
  std::unique_ptr<HelpBubbleViews> CreateHelpBubble(
      HelpBubbleParams params,
      ui::TrackedElement* element) {
    HelpBubbleView* const bubble_view =
        CreateHelpBubbleView(std::move(params), element->GetScreenBounds());
    return base::WrapUnique(new HelpBubbleViews(bubble_view, element));
  }

  void SetUp() override {
    HelpBubbleViewTest::SetUp();

    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kRightCenter;

    gfx::Rect anchor_bounds = GetWidgetClientBounds();
    anchor_bounds.Inset(50);

    test_element_ = std::make_unique<ui::test::TestElement>(
        kTestElementId, kTestElementContext);
    test_element_->SetScreenBounds(anchor_bounds);
    test_element_->Show();

    help_bubble_ = CreateHelpBubble(std::move(params), test_element_.get());
  }

  void TearDown() override {
    test_element_.reset();
    HelpBubbleViewTest::TearDown();
  }

 protected:
  gfx::Rect GetHelpBubbleAnchorRect() const {
    return help_bubble_->bubble_view()->GetAnchorRect();
  }

  std::unique_ptr<ui::test::TestElement> test_element_;
  std::unique_ptr<HelpBubbleViews> help_bubble_;
};

// This duplicates the HelpBubbleViewTest, but with a HelpBubbleViews object.
TEST_F(HelpBubbleViewsTest, AnchorToRect) {
  const auto widget_bounds = GetWidgetClientBounds();
  const auto anchor_bounds = test_element_->GetScreenBounds();
  const auto bubble_bounds = help_bubble_->GetBoundsInScreen();

  // The right side of the bubble should overlap the widget.
  EXPECT_TRUE(widget_bounds.Contains(bubble_bounds.right_center()));

  // The right side of the widget should be outside and aligned with the center
  // of the anchor bounds. Allow for rounding error when checking alignment.
  EXPECT_LT(bubble_bounds.right(), anchor_bounds.x());
  EXPECT_LE(std::abs(bubble_bounds.CenterPoint().y() -
                     anchor_bounds.CenterPoint().y()),
            2);
}

// This duplicates the HelpBubbleViewTest, but with a HelpBubbleViews object.
TEST_F(HelpBubbleViewsTest, AnchorRectUpdated) {
  const gfx::Rect old_bounds = help_bubble_->GetBoundsInScreen();

  // Move the anchor target by a small but noticeable amount.
  auto new_bounds = test_element_->GetScreenBounds();
  constexpr gfx::Vector2d kAnchorOffset{9, 13};
  new_bounds.Offset(kAnchorOffset);
  test_element_->SetScreenBounds(new_bounds);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      test_element_.get(), kHelpBubbleAnchorBoundsChangedEvent);

  // Verify that the help bubble has moved by a similar amount.
  gfx::Rect expected = old_bounds;
  expected.Offset(kAnchorOffset);
  EXPECT_EQ(expected, help_bubble_->GetBoundsInScreen());
}

// This checks a case where the target anchor region scrolls partially out of
// the host view. The anchor rect should be the intersection of the two.
TEST_F(HelpBubbleViewsTest, AnchorRectOverlapsEdge) {
  const gfx::Rect old_bounds = help_bubble_->GetBoundsInScreen();

  // Move the anchor target so that the upper left is beyond the edge of the
  // anchor view.
  auto new_bounds = test_element_->GetScreenBounds();
  new_bounds.Offset(-100, -100);
  test_element_->SetScreenBounds(new_bounds);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      test_element_.get(), kHelpBubbleAnchorBoundsChangedEvent);

  // Verify that the help bubble has moved.
  constexpr gfx::Rect kNewAnchorBounds{kWidgetBounds.x(), kWidgetBounds.y(), 50,
                                       50};
  EXPECT_EQ(kNewAnchorBounds, GetHelpBubbleAnchorRect());
  const gfx::Rect help_bubble_bounds = help_bubble_->GetBoundsInScreen();
  EXPECT_LT(help_bubble_bounds.y(), old_bounds.y());
  EXPECT_GT(help_bubble_bounds.CenterPoint().y(), kNewAnchorBounds.y());
  EXPECT_LT(help_bubble_bounds.CenterPoint().y(), kNewAnchorBounds.bottom());

  // Bubble may have mirrored horizontally. Check which orientation it's in and
  // verify the position is appropriate to the new anchor region.
  switch (help_bubble_->bubble_view()->GetBubbleFrameView()->GetArrow()) {
    case views::BubbleBorder::RIGHT_CENTER:
      EXPECT_LT(help_bubble_bounds.x(), old_bounds.x());
      EXPECT_LT(help_bubble_bounds.right(), kNewAnchorBounds.x());
      break;
    case views::BubbleBorder::LEFT_CENTER:
      EXPECT_GT(help_bubble_bounds.x(), old_bounds.x());
      EXPECT_GT(help_bubble_bounds.x(), kNewAnchorBounds.right());
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Arrow should only be right-center or left-center.";
  }
}

// This checks a case where the target anchor region scrolls fully out of
// the host view. The anchor rect should be a one-pixel slice on the edge
// closest to the actual anchor.
TEST_F(HelpBubbleViewsTest, AnchorOutsideBoundsHorizontal) {
  const gfx::Rect old_bounds = help_bubble_->GetBoundsInScreen();

  // Move the anchor target entirely off the right side of the anchor view.
  auto new_bounds = test_element_->GetScreenBounds();
  new_bounds.Offset(200, 0);
  test_element_->SetScreenBounds(new_bounds);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      test_element_.get(), kHelpBubbleAnchorBoundsChangedEvent);

  // Verify that the help bubble has moved. It might be mirrored, however.
  constexpr gfx::Rect kNewAnchorBounds{kWidgetBounds.right() - 1,
                                       kWidgetBounds.y() + 50, 1, 100};
  EXPECT_EQ(kNewAnchorBounds, GetHelpBubbleAnchorRect());
  const gfx::Rect help_bubble_bounds = help_bubble_->GetBoundsInScreen();
  EXPECT_EQ(help_bubble_bounds.y(), old_bounds.y());
  EXPECT_GT(help_bubble_bounds.x(), old_bounds.x());
  EXPECT_LT(help_bubble_bounds.right(), kNewAnchorBounds.x());
}

// This checks a case where the target anchor region scrolls fully out of
// the host view. The anchor rect should be a one-pixel slice on the edge
// closest to the actual anchor.
TEST_F(HelpBubbleViewsTest, AnchorOutsideBoundsVertical) {
  const gfx::Rect old_bounds = help_bubble_->GetBoundsInScreen();

  // Move the anchor target entirely beyond the bottom of the anchor view.
  auto new_bounds = test_element_->GetScreenBounds();
  new_bounds.Offset(0, 200);
  test_element_->SetScreenBounds(new_bounds);
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      test_element_.get(), kHelpBubbleAnchorBoundsChangedEvent);

  // Verify that the help bubble has moved. It might be mirrored, however.
  constexpr gfx::Rect kNewAnchorBounds{kWidgetBounds.x() + 50,
                                       kWidgetBounds.bottom() - 1, 100, 1};
  EXPECT_EQ(kNewAnchorBounds, GetHelpBubbleAnchorRect());
  const gfx::Rect help_bubble_bounds = help_bubble_->GetBoundsInScreen();
  EXPECT_EQ(help_bubble_bounds.x(), old_bounds.x());
  EXPECT_GT(help_bubble_bounds.y(), old_bounds.y());
  EXPECT_LT(help_bubble_bounds.y(), kNewAnchorBounds.y());
  EXPECT_GE(help_bubble_bounds.bottom(), kNewAnchorBounds.y());
  EXPECT_LT(help_bubble_bounds.right(), kNewAnchorBounds.x());
}

// Verifies that a bubble anchored to a region will still move with the owning
// Widget.
TEST_F(HelpBubbleViewsTest, MoveAnchorWidget) {
  const auto old_bubble_bounds = help_bubble_->GetBoundsInScreen();
  gfx::Rect widget_bounds = widget_->GetWindowBoundsInScreen();
  constexpr gfx::Vector2d kOffset{9, 13};
  widget_bounds.Offset(kOffset);
  widget_->SetBounds(widget_bounds);
  gfx::Rect expected = old_bubble_bounds;
  expected.Offset(kOffset);
  EXPECT_EQ(expected, help_bubble_->GetBoundsInScreen());
}

}  // namespace user_education
