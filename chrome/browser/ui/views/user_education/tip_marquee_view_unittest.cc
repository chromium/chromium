// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/tip_marquee_view.h"
#include <cstddef>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

static constexpr gfx::Size kTipMarqueeWidgetSize(1000, 60);
static constexpr gfx::Size kSpacerPreferredSize(100, 60);

class LearnMoreCallback {
 public:
  TipMarqueeView::LearnMoreLinkClickedCallback Callback() {
    return base::BindRepeating(&LearnMoreCallback::IncrementCount,
                               base::Unretained(this));
  }
  int count() const { return count_; }

 private:
  void IncrementCount(TipMarqueeView*) { ++count_; }
  int count_ = 0;
};

}  // anonymous namespace

class TipMarqueeViewTest : public views::ViewsTestBase {
 public:
  TipMarqueeViewTest() = default;
  ~TipMarqueeViewTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(gfx::Point(), kTipMarqueeWidgetSize);
    widget_->Init(std::move(params));
    widget_->SetContentsView(
        views::Builder<views::FlexLayoutView>()
            .SetOrientation(views::LayoutOrientation::kHorizontal)
            .AddChildren(
                views::Builder<views::View>()
                    .CopyAddressTo(&spacer_)
                    .SetPreferredSize(kSpacerPreferredSize)
                    .SetProperty(views::kFlexBehaviorKey,
                                 views::FlexSpecification(
                                     views::LayoutOrientation::kHorizontal,
                                     views::MinimumFlexSizeRule::kPreferred,
                                     views::MaximumFlexSizeRule::kUnbounded)),
                views::Builder<TipMarqueeView>(
                    std::make_unique<TipMarqueeView>())
                    .CopyAddressTo(&marquee_)
                    .SetProperty(
                        views::kFlexBehaviorKey,
                        views::FlexSpecification(
                            views::LayoutOrientation::kHorizontal,
                            views::MinimumFlexSizeRule::kPreferredSnapToMinimum)
                            .WithOrder(2))
                    .SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kCenter))
            .Build());

    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    spacer_ = nullptr;
    marquee_ = nullptr;
    ViewsTestBase::TearDown();
  }

  // Send mouse down and mouse up event at |point| within the marquee, ensuring
  // that the event is delivered to the correct view (which could be the marquee
  // or a child view).
  void SimulateMarqueeClick(gfx::Point point) {
    point.Offset(marquee_->x(), marquee_->y());
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point, point,
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    widget_->GetRootView()->OnMouseEvent(&press);
    widget_->GetRootView()->OnMouseEvent(&release);
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> spacer_ = nullptr;
  raw_ptr<TipMarqueeView> marquee_ = nullptr;
};

TEST_F(TipMarqueeViewTest, NotVisibleWhenNoTip) {
  views::test::RunScheduledLayout(marquee_);
  EXPECT_FALSE(marquee_->GetVisible());
}

TEST_F(TipMarqueeViewTest, VisibleWhenTipSet) {
  marquee_->SetAndShowTip(u"Tip Text");
  views::test::RunScheduledLayout(marquee_);
  EXPECT_TRUE(marquee_->GetVisible());
}

TEST_F(TipMarqueeViewTest, ClearTipHidesView) {
  marquee_->SetAndShowTip(u"Tip Text");
  views::test::RunScheduledLayout(marquee_);
  EXPECT_TRUE(marquee_->GetVisible());
  EXPECT_EQ(marquee_->GetPreferredSize(), marquee_->size());
  marquee_->ClearAndHideTip();
  views::test::RunScheduledLayout(marquee_);
  EXPECT_FALSE(marquee_->GetVisible());
}

TEST_F(TipMarqueeViewTest, TipStartsExpanded) {
  marquee_->SetAndShowTip(u"Tip Text");
  views::test::RunScheduledLayout(marquee_);
  EXPECT_GT(marquee_->width(), marquee_->GetMinimumSize().width());
}

TEST_F(TipMarqueeViewTest, TipCollapsesWhenNotEnoughSpace) {
  marquee_->SetAndShowTip(u"Tip Text");
  views::test::RunScheduledLayout(marquee_);
  gfx::Size spacer_size = spacer_->size();
  spacer_size.Enlarge(1, 0);
  spacer_->SetPreferredSize(spacer_size);
  views::test::RunScheduledLayout(marquee_);
  EXPECT_EQ(marquee_->width(), marquee_->GetMinimumSize().width());
}

TEST_F(TipMarqueeViewTest, TipCollapsesAndExpandsWhenIconIsClicked) {
  marquee_->SetAndShowTip(u"Tip Text");
  views::test::RunScheduledLayout(marquee_);

  // This location should be comfortably inside the icon area.
  constexpr gfx::Point kPressPoint(10, 10);

  // Collapse.
  marquee_->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, kPressPoint, kPressPoint, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  views::test::RunScheduledLayout(marquee_);
  EXPECT_EQ(marquee_->width(), marquee_->GetMinimumSize().width());

  // Expand.
  marquee_->OnMousePressed(ui::MouseEvent(
      ui::ET_MOUSE_PRESSED, kPressPoint, kPressPoint, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  views::test::RunScheduledLayout(marquee_);
  EXPECT_GT(marquee_->width(), marquee_->GetMinimumSize().width());
}

TEST_F(TipMarqueeViewTest, TipDoesNotExpandWhenInsufficientSpace) {
  marquee_->SetAndShowTip(u"Tip Text");
  views::test::RunScheduledLayout(marquee_);
  gfx::Size spacer_size = spacer_->size();
  spacer_size.Enlarge(1, 0);
  spacer_->SetPreferredSize(spacer_size);
  views::test::RunScheduledLayout(marquee_);
  EXPECT_EQ(marquee_->width(), marquee_->GetMinimumSize().width());

  // This location should be comfortably inside the icon area.
  constexpr gfx::Point kPressPoint(10, 10);
  SimulateMarqueeClick(kPressPoint);
  views::test::RunScheduledLayout(marquee_);
  EXPECT_EQ(marquee_->width(), marquee_->GetMinimumSize().width());
}

TEST_F(TipMarqueeViewTest, ClickLearnMoreLink) {
  LearnMoreCallback callback;
  marquee_->SetAndShowTip(u"Tip Text", callback.Callback());
  views::test::RunScheduledLayout(marquee_);
  EXPECT_GT(marquee_->width(), marquee_->GetMinimumSize().width());

  // This location should be comfortably inside the "learn more" link.
  const gfx::Point click_point(marquee_->width() - 10, marquee_->height() / 2);
  EXPECT_EQ(0, callback.count());
  SimulateMarqueeClick(click_point);
  EXPECT_EQ(1, callback.count());
}

TEST_F(TipMarqueeViewTest, ClickNotInLearnMoreLinkHasNoEffect) {
  LearnMoreCallback callback;
  marquee_->SetAndShowTip(u"Tip Text", callback.Callback());
  views::test::RunScheduledLayout(marquee_);
  EXPECT_GT(marquee_->width(), marquee_->GetMinimumSize().width());

  // This location should be comfortably inside the tip text but not the link.
  const gfx::Point click_point(TipMarqueeView::kTipMarqueeIconTotalWidth + 10,
                               marquee_->height() / 2);
  EXPECT_EQ(0, callback.count());
  SimulateMarqueeClick(click_point);
  EXPECT_EQ(0, callback.count());
}

TEST_F(TipMarqueeViewTest, ClickWhenForcedCollapsedCallsLearnMore) {
  LearnMoreCallback callback;
  marquee_->SetAndShowTip(u"Tip Text", callback.Callback());
  views::test::RunScheduledLayout(marquee_);
  gfx::Size spacer_size = spacer_->size();
  spacer_size.Enlarge(1, 0);
  spacer_->SetPreferredSize(spacer_size);
  views::test::RunScheduledLayout(marquee_);
  EXPECT_EQ(marquee_->width(), marquee_->GetMinimumSize().width());

  // This location should be comfortably inside the icon area.
  constexpr gfx::Point kPressPoint(10, 10);
  EXPECT_EQ(0, callback.count());
  SimulateMarqueeClick(kPressPoint);
  EXPECT_EQ(1, callback.count());
}

TEST_F(TipMarqueeViewTest, ClickWhenForcedCollapsedDisplaysOverflow) {
  marquee_->SetAndShowTip(u"Tip Text");
  views::test::RunScheduledLayout(marquee_);
  gfx::Size spacer_size = spacer_->size();
  spacer_size.Enlarge(1, 0);
  spacer_->SetPreferredSize(spacer_size);
  views::test::RunScheduledLayout(marquee_);
  ASSERT_EQ(marquee_->width(), marquee_->GetMinimumSize().width());

  // This location should be comfortably inside the icon area.
  constexpr gfx::Point kPressPoint(10, 10);
  SimulateMarqueeClick(kPressPoint);
  views::DialogDelegate* const delegate =
      marquee_->GetProperty(views::kAnchoredDialogKey);
  EXPECT_NE(static_cast<views::DialogDelegate*>(nullptr), delegate);
}

TEST_F(TipMarqueeViewTest, OverflowBubbleCancelDoesNotDismissTip) {
  marquee_->SetAndShowTip(u"Tip Text");
  views::test::RunScheduledLayout(marquee_);
  gfx::Size spacer_size = spacer_->size();
  spacer_size.Enlarge(1, 0);
  spacer_->SetPreferredSize(spacer_size);
  views::test::RunScheduledLayout(marquee_);
  ASSERT_EQ(marquee_->width(), marquee_->GetMinimumSize().width());

  // This location should be comfortably inside the icon area.
  constexpr gfx::Point kPressPoint(10, 10);
  SimulateMarqueeClick(kPressPoint);
  views::DialogDelegate* const delegate =
      marquee_->GetProperty(views::kAnchoredDialogKey);
  views::Widget* const overflow_widget = delegate->GetWidget();
  ASSERT_NE(static_cast<views::Widget*>(nullptr), overflow_widget);
  views::test::WidgetDestroyedWaiter waiter(overflow_widget);
  ui::KeyEvent press_esc(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, 0);
  overflow_widget->OnKeyEvent(&press_esc);
  waiter.Wait();
  EXPECT_TRUE(marquee_->GetVisible());
}

TEST_F(TipMarqueeViewTest, OverflowBubbleGotItDismissesTip) {
  marquee_->SetAndShowTip(u"Tip Text");
  views::test::RunScheduledLayout(marquee_);
  gfx::Size spacer_size = spacer_->size();
  spacer_size.Enlarge(1, 0);
  spacer_->SetPreferredSize(spacer_size);
  views::test::RunScheduledLayout(marquee_);
  ASSERT_EQ(marquee_->width(), marquee_->GetMinimumSize().width());

  // This location should be comfortably inside the icon area.
  constexpr gfx::Point kPressPoint(10, 10);
  SimulateMarqueeClick(kPressPoint);
  views::DialogDelegate* const delegate =
      marquee_->GetProperty(views::kAnchoredDialogKey);
  views::test::WidgetDestroyedWaiter waiter(delegate->GetWidget());
  delegate->AcceptDialog();
  waiter.Wait();
  EXPECT_FALSE(marquee_->GetVisible());
}
