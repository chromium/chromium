// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"  // mash-ok
#include "ui/base/ui_base_features.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/ui/test/test_browser_dialog_mac.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_observer.h"
#endif

namespace {

#if defined(TOOLKIT_VIEWS)
// Helper to close a Widget.
class WidgetCloser {
 public:
  WidgetCloser(views::Widget* widget, bool async) : widget_(widget) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WidgetCloser::CloseWidget,
                                  weak_ptr_factory_.GetWeakPtr(), async));
  }

 private:
  void CloseWidget(bool async) {
    if (async)
      widget_->Close();
    else
      widget_->CloseNow();
  }

  views::Widget* widget_;

  base::WeakPtrFactory<WidgetCloser> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WidgetCloser);
};
#endif  // defined(TOOLKIT_VIEWS)

}  // namespace

TestBrowserDialog::TestBrowserDialog() = default;
TestBrowserDialog::~TestBrowserDialog() = default;

void TestBrowserDialog::PreShow() {
  UpdateWidgets();
}

// This can return false if no dialog was shown, if the dialog shown wasn't a
// toolkit-views dialog, or if more than one child dialog was shown.
bool TestBrowserDialog::VerifyUi() {
#if defined(TOOLKIT_VIEWS)
  views::Widget::Widgets widgets_before = widgets_;
  UpdateWidgets();

  auto added =
      base::STLSetDifference<views::Widget::Widgets>(widgets_, widgets_before);

  if (added.size() > 1) {
    // Some tests create a standalone window to anchor a dialog. In those cases,
    // ignore added Widgets that are not dialogs.
    base::EraseIf(added, [](views::Widget* widget) {
      return !widget->widget_delegate()->AsDialogDelegate();
    });
  }
  widgets_ = added;

  if (added.size() != 1)
    return false;

  if (!should_verify_dialog_bounds_)
    return true;

  // Verify that the dialog's dimensions do not exceed the display's work area
  // bounds, which may be smaller than its bounds(), e.g. in the case of the
  // docked magnifier or Chromevox being enabled.
  views::Widget* dialog_widget = *(added.begin());
  const gfx::Rect dialog_bounds = dialog_widget->GetWindowBoundsInScreen();
  gfx::NativeWindow native_window = dialog_widget->GetNativeWindow();
  DCHECK(native_window);
  display::Screen* screen = display::Screen::GetScreen();
  const gfx::Rect display_work_area =
      screen->GetDisplayNearestWindow(native_window).work_area();

  return display_work_area.Contains(dialog_bounds);
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

void TestBrowserDialog::WaitForUserDismissal() {
#if defined(OS_MACOSX)
  internal::TestBrowserDialogInteractiveSetUp();
#endif

#if defined(TOOLKIT_VIEWS)
  ASSERT_FALSE(widgets_.empty());
  views::test::WidgetDestroyedWaiter waiter(*widgets_.begin());
  waiter.Wait();
#else
  NOTIMPLEMENTED();
#endif
}

void TestBrowserDialog::DismissUi() {
#if defined(TOOLKIT_VIEWS)
  ASSERT_FALSE(widgets_.empty());
  views::test::WidgetDestroyedWaiter waiter(*widgets_.begin());
  WidgetCloser closer(*widgets_.begin(), AlwaysCloseAsynchronously());
  waiter.Wait();
#else
  NOTIMPLEMENTED();
#endif
}

bool TestBrowserDialog::AlwaysCloseAsynchronously() {
  // TODO(tapted): Iterate over close methods for greater test coverage.
  return false;
}

void TestBrowserDialog::UpdateWidgets() {
  widgets_.clear();
#if defined(OS_CHROMEOS)
  // Under mash, GetAllWidgets() uses MusClient to get the list of root windows.
  // Otherwise, GetAllWidgets() relies on AuraTestHelper to get the root window,
  // but that is not available in browser_tests, so use ash::Shell directly.
  if (features::IsUsingWindowService()) {
    widgets_ = views::test::WidgetTest::GetAllWidgets();
  } else {
    for (aura::Window* root_window : ash::Shell::GetAllRootWindows())
      views::Widget::GetAllChildWidgets(root_window, &widgets_);
  }
#elif defined(TOOLKIT_VIEWS)
  widgets_ = views::test::WidgetTest::GetAllWidgets();
#else
  NOTIMPLEMENTED();
#endif
}
