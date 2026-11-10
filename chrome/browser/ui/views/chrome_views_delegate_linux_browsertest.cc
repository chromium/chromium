// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

namespace {

struct WindowClass {
  std::string wm_class_name;
  std::string wm_class_class;
  std::string wayland_app_id;
};

class ChromeViewsDelegateLinuxBrowserTest : public InProcessBrowserTest {
 protected:
  // Initializes a widget with `params` and returns the window class that the
  // native widget is created with.
  WindowClass InitWidgetAndGetWindowClass(views::Widget::InitParams params) {
    views::ViewsDelegate* views_delegate = views::ViewsDelegate::GetInstance();
    WindowClass window_class;
    // Record the params the native widget is created with, and return null so
    // that the default native widget is created.
    views_delegate->set_native_widget_factory(base::BindRepeating(
        [](WindowClass* window_class, const views::Widget::InitParams& params,
           views::internal::NativeWidgetDelegate*) -> views::NativeWidget* {
          window_class->wm_class_name = params.wm_class_name;
          window_class->wm_class_class = params.wm_class_class;
          window_class->wayland_app_id = params.wayland_app_id;
          return nullptr;
        },
        &window_class));

    params.bounds = gfx::Rect(0, 0, 100, 100);
    auto widget = std::make_unique<views::Widget>();
    widget->Init(std::move(params));
    views_delegate->set_native_widget_factory({});
    widget->CloseNow();
    return window_class;
  }
};

// Top-level windows that don't set a window class, such as the video
// Picture-in-Picture window, get the browser's window class so that taskbars
// group them with the browser windows.
IN_PROC_BROWSER_TEST_F(ChromeViewsDelegateLinuxBrowserTest,
                       DefaultWindowClass) {
  const WindowClass window_class = InitWidgetAndGetWindowClass(
      views::Widget::InitParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                                views::Widget::InitParams::TYPE_WINDOW));
  EXPECT_FALSE(window_class.wm_class_class.empty());
  EXPECT_EQ(window_class.wm_class_name,
            shell_integration_linux::GetProgramClassName());
  EXPECT_EQ(window_class.wm_class_class,
            shell_integration_linux::GetProgramClassClass());
  EXPECT_EQ(window_class.wayland_app_id, window_class.wm_class_class);
}

// A window class set by the widget's creator is kept.
IN_PROC_BROWSER_TEST_F(ChromeViewsDelegateLinuxBrowserTest,
                       ExplicitWindowClassIsKept) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.wm_class_name = "name";
  params.wm_class_class = "Class";
  params.wayland_app_id = "app-id";
  const WindowClass window_class =
      InitWidgetAndGetWindowClass(std::move(params));
  EXPECT_EQ(window_class.wm_class_name, "name");
  EXPECT_EQ(window_class.wm_class_class, "Class");
  EXPECT_EQ(window_class.wayland_app_id, "app-id");
}

// Fields that the widget's creator leaves empty get the browser's values,
// while the ones it sets are kept.
IN_PROC_BROWSER_TEST_F(ChromeViewsDelegateLinuxBrowserTest,
                       PartialWindowClassIsFilledIn) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.wm_class_name = "name";
  params.wm_class_class = "Class";
  const WindowClass window_class =
      InitWidgetAndGetWindowClass(std::move(params));
  EXPECT_EQ(window_class.wm_class_name, "name");
  EXPECT_EQ(window_class.wm_class_class, "Class");
  EXPECT_EQ(window_class.wayland_app_id,
            shell_integration_linux::GetProgramClassClass());
}

}  // namespace
