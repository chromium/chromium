// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/intent_helper/custom_tab.h"

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

// Bypasses Views-based clipping that breaks inside Exo/Wayland shells.  Ensures
// that CustomTab forces its NativeViewHost to use the legacy ClippingWindow
// architecture. See b/559463652.
TEST_F(CustomTabTest, LayerManagedByViewsIsDisabled) {
  views::TestViewsDelegate views_delegate;

  views::Widget toplevel_widget;
  {
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.context = root_window();
    toplevel_widget.Init(std::move(params));
  }
  auto custom_tab =
      std::make_unique<CustomTab>(toplevel_widget.GetNativeWindow());

  views::View* contents_view = toplevel_widget.GetContentsView();
  ASSERT_TRUE(contents_view);
  ASSERT_FALSE(contents_view->children().empty());

  // CustomTab attaches a NativeViewHost to the widget's contents view.
  views::View* host_view = nullptr;
  for (views::View* child : contents_view->children()) {
    if (child->GetClassName() == std::string_view("NativeViewHost")) {
      host_view = child;
      break;
    }
  }
  ASSERT_TRUE(host_view) << "NativeViewHost not found in contents_view";

  auto* host = static_cast<views::NativeViewHost*>(host_view);

  // Structural Regression Guard:
  // CustomTab must explicitly disable layer management to avoid severe clipping
  // bugs when embedded inside an Exo Wayland shell surface due to coordinate
  // bounds mismatch. While an end-to-end pixel test is visually ideal, reliably
  // mocking the intersection of Wayland, Aura, and Android UI surfaces in C++
  // is highly brittle and prone to flakiness. By explicitly asserting this
  // structural implementation state instead of the behavioral outcome, we
  // establish a firm architectural guardrail. This guarantees the legacy
  // clipping fallback architecture is preserved and cleanly prevents future
  // refactors from silently triggering the visual regression.
  EXPECT_FALSE(host->layer_managed_by_views());
}

}  // namespace arc
