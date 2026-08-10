// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_overlay_view.h"

#include <memory>

#include "chrome/browser/dictation/test_util.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace dictation {

class DictationOverlayViewTest : public ChromeViewsTestBase {
 public:
  DictationOverlayViewTest() = default;
  DictationOverlayViewTest(const DictationOverlayViewTest&) = delete;
  DictationOverlayViewTest& operator=(const DictationOverlayViewTest&) = delete;
  ~DictationOverlayViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    parent_widget_->Show();
  }

  void TearDown() override {
    parent_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> parent_widget_;
  base::test::ScopedFeatureList scoped_feature_list_{
      CreateEnablingFeatureList()};
};

TEST_F(DictationOverlayViewTest, ShowAndReposition) {
  auto overlay =
      std::make_unique<DictationOverlayView>(parent_widget_->GetNativeView());

  overlay->Show();
  views::Widget* widget = overlay->GetWidget();
  ASSERT_NE(widget, nullptr);
  EXPECT_TRUE(widget->IsVisible());

  gfx::Point selection_point(100, 200);
  overlay->UpdatePosition(selection_point);
  EXPECT_EQ(overlay->GetAnchorRect(), gfx::Rect(selection_point, gfx::Size()));
}

}  // namespace dictation
