// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_window_manager_aura.h"
#include "ui/aura/window.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace chromecast {
namespace test {

using CastViewsTest = views::ViewsTestBase;

TEST_F(CastViewsTest, ProgressBar) {
  std::unique_ptr<CastWindowManager> window_manager =
      std::make_unique<CastWindowManagerAura>(true /* enable input */);
  gfx::Rect bounds = window_manager->GetRootWindow()->bounds();

  views::ProgressBar* progress_bar = new views::ProgressBar();
  progress_bar->SetPreferredHeight(bounds.height());
  progress_bar->SetValue(0.5);

  // Create the window.  We close the window by deleting it, so we take
  // ownership of the widget + native widget.
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  params.context = window_manager->GetRootWindow();
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.bounds = bounds;
  std::unique_ptr<views::Widget> widget(new views::Widget);
  widget->Init(std::move(params));
  widget->SetOpacity(0.6);
  widget->SetContentsView(progress_bar);
  window_manager->SetZOrder(widget->GetNativeView(), mojom::ZOrder::VOLUME);
  widget->Show();

  EXPECT_TRUE(progress_bar->GetWidget());
  EXPECT_TRUE(progress_bar->GetWidget()->IsVisible());
  EXPECT_TRUE(progress_bar->GetVisible());
  EXPECT_TRUE(progress_bar->GetEnabled());

  widget.reset();
  window_manager.reset();
}

}  // namespace test
}  // namespace chromecast
