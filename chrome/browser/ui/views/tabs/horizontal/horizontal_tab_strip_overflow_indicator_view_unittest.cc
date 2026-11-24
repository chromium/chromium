// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_strip_overflow_indicator_view.h"

#include <memory>

#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/paint_info.h"
#include "ui/views/widget/widget.h"

class HorizontalTabStripOverflowIndicatorViewTest : public ChromeViewsTestBase {
 protected:
  SkBitmap RenderIndicator(views::OverflowIndicatorAlignment side,
                           int inset,
                           int& center_y) {
    auto widget =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* indicator = widget->SetContentsView(
        std::make_unique<HorizontalTabStripOverflowIndicatorView>(side));
    indicator->SetOverlap(inset);
    widget->SetBounds(gfx::Rect(
        0, 0, HorizontalTabStripOverflowIndicatorView::GetMaxTotalThickness(),
        40));
    widget->ShowInactive();
    center_y = indicator->GetContentsBounds().CenterPoint().y();
    SkBitmap bitmap;
    ui::CanvasPainter painter(&bitmap, indicator->size(), 1.0f,
                              SK_ColorTRANSPARENT, /*is_pixel_canvas=*/true);
    indicator->Paint(views::PaintInfo::CreateRootPaintInfo(painter.context(),
                                                           indicator->size()));
    return bitmap;
  }
};

TEST_F(HorizontalTabStripOverflowIndicatorViewTest,
       PaintsOnlyShadowGradientPastTransparentOverlapInset) {
  const int inset = TabStyle::Get()->GetBottomExtensionWidth();
  const int shadow = HorizontalTabStripOverflowIndicatorView::kShadowThickness;
  int y = 0;

  const SkBitmap bitmap =
      RenderIndicator(views::OverflowIndicatorAlignment::kLeft, inset, y);

  for (int x = 0; x < bitmap.width(); ++x) {
    if (x < inset || x >= inset + shadow) {
      EXPECT_EQ(SkColorGetA(bitmap.getColor(x, y)), SK_AlphaTRANSPARENT);
    }
  }
  EXPECT_GT(SkColorGetA(bitmap.getColor(inset, y)),
            SkColorGetA(bitmap.getColor(inset + shadow - 1, y)));
  if (GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap) > 0) {
    EXPECT_EQ(SkColorGetA(bitmap.getColor(inset, 39)), SK_AlphaTRANSPARENT);
  }
}

TEST_F(HorizontalTabStripOverflowIndicatorViewTest,
       MirrorsShadowAndTransparentInsetInRTL) {
  base::i18n::ScopedRTLForTesting rtl(true);
  const int inset = TabStyle::Get()->GetTabOverlap() / 2;
  const int shadow = HorizontalTabStripOverflowIndicatorView::kShadowThickness;
  int y = 0;

  const SkBitmap bitmap =
      RenderIndicator(views::OverflowIndicatorAlignment::kLeft, inset, y);

  const int width = bitmap.width();
  for (int x = 0; x < width; ++x) {
    if (x < width - inset - shadow || x >= width - inset) {
      EXPECT_EQ(SkColorGetA(bitmap.getColor(x, y)), SK_AlphaTRANSPARENT);
    }
  }
  EXPECT_GT(SkColorGetA(bitmap.getColor(width - inset - 1, y)),
            SkColorGetA(bitmap.getColor(width - inset - shadow, y)));
}
