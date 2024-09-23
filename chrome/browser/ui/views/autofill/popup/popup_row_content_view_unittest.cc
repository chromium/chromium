// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {

class PopupRowContentViewTest : public ChromeViewsTestBase {
 public:
  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
  }

  void ShowView(std::unique_ptr<PopupRowContentView> cell_view) {
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

  std::unique_ptr<PopupRowContentView> CreatePopupCellView() {
    return std::make_unique<PopupRowContentView>();
  }

 protected:
  ui::test::EventGenerator& generator() { return *generator_; }
  PopupRowContentView& view() { return *view_; }
  views::Widget& widget() { return *widget_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  raw_ptr<PopupRowContentView> view_ = nullptr;
};

TEST_F(PopupRowContentViewTest, SetSelectedUpdatesBackground) {
  ShowView(std::make_unique<PopupRowContentView>());

  views::Background* background = view().GetBackground();
  ASSERT_FALSE(background);

  view().UpdateStyle(true);
  background = view().GetBackground();
  ASSERT_TRUE(background);
  EXPECT_EQ(background->get_color(), view().GetColorProvider()->GetColor(
                                         ui::kColorDropdownBackgroundSelected));
}

}  // namespace autofill
