// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/custom_tab.h"

#include <memory>

#include "ui/aura/test/aura_test_base.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"

namespace arc {

using CustomTabTest = aura::test::AuraTestBase;

// Make sure resizing the widget after closing custom tab will not crash.
// b/169014289
TEST_F(CustomTabTest, ResizeAfterClose) {
  views::TestViewsDelegate views_delegate;

  views::Widget toplevel_widget;
  {
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.activatable = views::Widget::InitParams::Activatable::kYes;
    params.bounds = gfx::Rect(0, 0, 200, 200);
    params.parent = root_window();
    toplevel_widget.Init(std::move(params));
  }
  auto custom_tab =
      std::make_unique<CustomTab>(toplevel_widget.GetNativeWindow());

  views::Widget embedded_widget;
  {
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_CONTROL);
    params.context = root_window();
    embedded_widget.Init(std::move(params));
    embedded_widget.Show();
  }
  custom_tab->Attach(embedded_widget.GetNativeWindow());
  toplevel_widget.Show();

  custom_tab.reset();
  // Resize to force re-layout child views to make sure that deleting the custom
  // tab removes the native view host inside upon deletion.
  toplevel_widget.SetSize(gfx::Size(250, 250));
}

}  // namespace arc
