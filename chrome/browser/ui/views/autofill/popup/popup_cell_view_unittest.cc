// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/autofill/popup/test_popup_row_strategy.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/color/color_id.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

using testing::StrictMock;

namespace autofill {

namespace {
constexpr gfx::Point kOutOfBounds{1000, 1000};
}  // namespace

class PopupCellViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void ShowView(std::unique_ptr<PopupCellView> cell_view) {
    view_ = widget_->SetContentsView(std::move(cell_view));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void Paint() {
    views::View& paint_view = *widget().GetRootView();
    SkBitmap bitmap;
    gfx::Size size = paint_view.size();
    ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, SK_ColorTRANSPARENT,
                                     false);
    paint_view.Paint(
        views::PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
  }

 protected:
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupCellView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  raw_ptr<PopupCellView> view_ = nullptr;
};

TEST_F(PopupCellViewTest, AccessibleNodeData) {
  ShowView(views::Builder<PopupCellView>()
               .SetAccessibilityDelegate(
                   std::make_unique<TestAccessibilityDelegate>())
               .Build());

  ui::AXNodeData node_data;
  view().GetAccessibleNodeData(&node_data);

  EXPECT_EQ(TestAccessibilityDelegate::kVoiceOverName,
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PopupCellViewTest, SetSelectedUpdatesBackground) {
  ShowView(views::Builder<PopupCellView>()
               .SetAccessibilityDelegate(
                   std::make_unique<TestAccessibilityDelegate>())
               .Build());

  // The unselected background.
  EXPECT_FALSE(view().GetSelected());
  views::Background* background = view().GetBackground();
  ASSERT_TRUE(background);
  EXPECT_EQ(background->get_color(),
            view().GetColorProvider()->GetColor(ui::kColorDropdownBackground));

  view().SetSelected(true);
  EXPECT_TRUE(view().GetSelected());
  background = view().GetBackground();
  ASSERT_TRUE(background);
  EXPECT_EQ(background->get_color(), view().GetColorProvider()->GetColor(
                                         ui::kColorDropdownBackgroundSelected));
}

TEST_F(PopupCellViewTest, Tooltip) {
  constexpr char16_t kTooltip[] = u"Sample tooltip";

  ShowView(views::Builder<PopupCellView>()
               .SetAccessibilityDelegate(
                   std::make_unique<TestAccessibilityDelegate>())
               .SetTooltipText(kTooltip)
               .Build());
  EXPECT_EQ(view().GetTooltipText(), kTooltip);

  // The method derived form `views::View` responds properly, too.
  const views::View& cell_as_view = view();
  EXPECT_EQ(cell_as_view.GetTooltipText(gfx::Point()), kTooltip);
}

TEST_F(PopupCellViewTest, SetSelectedUpdatesTrackedLabels) {
  std::unique_ptr<PopupCellView> cell =
      views::Builder<PopupCellView>()
          .SetAccessibilityDelegate(
              std::make_unique<TestAccessibilityDelegate>())
          .Build();
  views::Label* tracked_label =
      cell->AddChildView(std::make_unique<views::Label>(
          u"Label text 1", views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
  views::Label* untracked_label =
      cell->AddChildView(std::make_unique<views::Label>(
          u"Label text 2", views::style::CONTEXT_DIALOG_BODY_TEXT,
          views::style::STYLE_SECONDARY));
  cell->TrackLabel(tracked_label);
  ShowView(std::move(cell));

  auto get_expected_color = [](views::Label& label, int style) {
    return label.GetColorProvider()->GetColor(
        views::style::GetColorId(label.GetTextContext(), style));
  };

  // The unselected state.
  EXPECT_FALSE(view().GetSelected());
  EXPECT_EQ(tracked_label->GetEnabledColor(),
            get_expected_color(*tracked_label, tracked_label->GetTextStyle()));
  EXPECT_EQ(
      untracked_label->GetEnabledColor(),
      get_expected_color(*untracked_label, untracked_label->GetTextStyle()));

  // // On select updates only the tracked label's style.
  view().SetSelected(true);
  EXPECT_TRUE(view().GetSelected());
  EXPECT_NE(
      tracked_label->GetEnabledColor(),
      get_expected_color(*tracked_label, untracked_label->GetTextStyle()));
  EXPECT_EQ(tracked_label->GetEnabledColor(),
            get_expected_color(*tracked_label, views::style::STYLE_SELECTED));
  EXPECT_EQ(
      untracked_label->GetEnabledColor(),
      get_expected_color(*untracked_label, untracked_label->GetTextStyle()));
}

TEST_F(PopupCellViewTest, MouseEvents) {
  std::unique_ptr<PopupCellView> cell =
      views::Builder<PopupCellView>()
          .SetAccessibilityDelegate(
              std::make_unique<TestAccessibilityDelegate>())
          .Build();
  views::Label* label =
      cell->AddChildView(std::make_unique<views::Label>(u"Label text"));
  ShowView(std::move(cell));

  StrictMock<base::MockCallback<base::RepeatingClosure>> enter_callback;
  StrictMock<base::MockCallback<base::RepeatingClosure>> exit_callback;
  StrictMock<base::MockCallback<base::RepeatingClosure>> accept_callback;

  generator().MoveMouseTo(kOutOfBounds);
  ASSERT_FALSE(view().IsMouseHovered());
  generator().MoveMouseTo(label->GetBoundsInScreen().CenterPoint());
  ASSERT_TRUE(view().IsMouseHovered());
  generator().ClickLeftButton();
  generator().MoveMouseTo(kOutOfBounds);
  ASSERT_FALSE(view().IsMouseHovered());
  Paint();

  view().SetOnEnteredCallback(enter_callback.Get());
  view().SetOnExitedCallback(exit_callback.Get());
  view().SetOnAcceptedCallback(accept_callback.Get());
  EXPECT_CALL(enter_callback, Run);
  generator().MoveMouseTo(label->GetBoundsInScreen().CenterPoint());
  EXPECT_CALL(accept_callback, Run);
  generator().ClickLeftButton();
  EXPECT_CALL(exit_callback, Run);
  generator().MoveMouseTo(kOutOfBounds);
}

// Gestures are not supported on MacOS.
#if !BUILDFLAG(IS_MAC)
TEST_F(PopupCellViewTest, GestureEvents) {
  std::unique_ptr<PopupCellView> cell =
      views::Builder<PopupCellView>()
          .SetAccessibilityDelegate(
              std::make_unique<TestAccessibilityDelegate>())
          .Build();
  views::Label* label =
      cell->AddChildView(std::make_unique<views::Label>(u"Label text"));
  ShowView(std::move(cell));

  StrictMock<base::MockCallback<base::RepeatingClosure>> enter_callback;
  StrictMock<base::MockCallback<base::RepeatingClosure>> exit_callback;
  StrictMock<base::MockCallback<base::RepeatingClosure>> accept_callback;

  view().SetOnEnteredCallback(enter_callback.Get());
  view().SetOnExitedCallback(exit_callback.Get());
  view().SetOnAcceptedCallback(accept_callback.Get());

  EXPECT_CALL(enter_callback, Run);
  EXPECT_CALL(accept_callback, Run);
  generator().GestureTapAt(label->GetBoundsInScreen().CenterPoint());
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_F(PopupCellViewTest, IgnoreClickIfMouseWasNotOutsideBefore) {
  std::unique_ptr<PopupCellView> cell =
      views::Builder<PopupCellView>()
          .SetAccessibilityDelegate(
              std::make_unique<TestAccessibilityDelegate>())
          .Build();
  views::Label* label =
      cell->AddChildView(std::make_unique<views::Label>(u"Label text"));
  ShowView(std::move(cell));

  StrictMock<base::MockCallback<base::RepeatingClosure>> accept_callback;

  view().SetOnAcceptedCallback(accept_callback.Get());
  generator().MoveMouseTo(label->GetBoundsInScreen().CenterPoint());
  Paint();
  // No OnAccept callback is run.
  generator().ClickLeftButton();

  generator().MoveMouseTo(kOutOfBounds);
  Paint();
  generator().MoveMouseTo(label->GetBoundsInScreen().CenterPoint());
  // If the mouse has been outside before, the accept click is passed through.
  EXPECT_CALL(accept_callback, Run);
  generator().ClickLeftButton();
}

}  // namespace autofill
