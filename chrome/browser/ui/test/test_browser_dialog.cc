// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_browser_dialog.h"

#include <set>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/test/test_browser_dialog_mac.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "base/strings/strcat.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/test/widget_test.h"
#endif

namespace {

#if defined(TOOLKIT_VIEWS)
// Helper to close a Widget.
class WidgetCloser {
 public:
  WidgetCloser(views::Widget* widget, bool async) : widget_(widget) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&WidgetCloser::CloseWidget,
                                  weak_ptr_factory_.GetWeakPtr(), async));
  }

  WidgetCloser(const WidgetCloser&) = delete;
  WidgetCloser& operator=(const WidgetCloser&) = delete;

 private:
  void CloseWidget(bool async) {
    if (async)
      widget_->Close();
    else
      widget_->CloseNow();
  }

  raw_ptr<views::Widget, AcrossTasksDanglingUntriaged> widget_;

  base::WeakPtrFactory<WidgetCloser> weak_ptr_factory_{this};
};

#endif  // defined(TOOLKIT_VIEWS)

}  // namespace

TestBrowserDialog::TestBrowserDialog() = default;

TestBrowserDialog::~TestBrowserDialog() = default;

void TestBrowserDialog::PreShow() {
  UpdateWidgets();
}

void TestBrowserDialog::ShowAndVerifyUi() {
  TestBrowserUi::ShowAndVerifyUi();
  baseline_.clear();
}

// This returns true if exactly one views widget was shown that is a dialog or
// has a name matching the test-specified name, and if that window is in the
// work area (if |should_verify_dialog_bounds_| is true).
bool TestBrowserDialog::VerifyUi() {
#if defined(TOOLKIT_VIEWS)
  views::Widget::Widgets widgets_before = widgets_;
  UpdateWidgets();

  // Force pending layouts of all existing widgets. This ensures any
  // anchor Views are in the correct position.
  for (views::Widget* widget : widgets_)
    widget->LayoutRootViewIfNecessary();

  // Get the list of added dialog widgets. Ignore non-dialog widgets, including
  // those added by tests to anchor dialogs and the browser's status bubble.
  // Non-dialog widgets matching the test-specified name will also be included.
  auto added =
      base::STLSetDifference<views::Widget::Widgets>(widgets_, widgets_before);
  std::string name = GetNonDialogName();
  std::erase_if(added, [&](views::Widget* widget) {
    return !widget->widget_delegate()->AsDialogDelegate() &&
           (name.empty() || widget->GetName() != name);
  });
  widgets_ = added;

  if (added.size() != 1) {
    LOG(INFO) << "VerifyUi(): Expected 1 added widget; got " << added.size();
    if (added.size() > 1) {
      std::u16string widget_title_log = u"Added Widgets are: ";
      for (views::Widget* widget : added) {
        widget_title_log += widget->widget_delegate()->GetWindowTitle() + u" ";
      }
      LOG(INFO) << widget_title_log;
    }
    return false;
  }

  views::Widget* dialog_widget = *(added.begin());
  dialog_widget->SetBlockCloseForTesting(true);
  // Deactivate before taking screenshot. Deactivated dialog pixel outputs
  // is more predictable than activated dialog.
  bool is_active = dialog_widget->IsActive();
  dialog_widget->Deactivate();
  dialog_widget->GetFocusManager()->ClearFocus();
  absl::Cleanup unblock_close = [dialog_widget] {
    dialog_widget->SetBlockCloseForTesting(false);
  };

  auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
  const std::string screenshot_name = base::StrCat(
      {test_info->test_suite_name(), "_", test_info->name(), "_", baseline_});

  if (VerifyPixelUi(dialog_widget, "BrowserUiDialog", screenshot_name) ==
      ui::test::ActionResult::kFailed) {
    LOG(INFO) << "VerifyUi(): Pixel compare failed.";
    return false;
  }
  if (is_active)
    dialog_widget->Activate();

  if (!should_verify_dialog_bounds_)
    return true;

  // Verify that the dialog's dimensions do not exceed the display's work area
  // bounds, which may be smaller than its bounds(), e.g. in the case of the
  // docked magnifier or Chromevox being enabled.
  const gfx::Rect dialog_bounds = dialog_widget->GetWindowBoundsInScreen();
  gfx::NativeWindow native_window = dialog_widget->GetNativeWindow();
  DCHECK(native_window);
  display::Screen* screen = display::Screen::GetScreen();
  const gfx::Rect display_work_area =
      screen->GetDisplayNearestWindow(native_window).work_area();

  const bool dialog_in_bounds = display_work_area.Contains(dialog_bounds);
  LOG_IF(INFO, !dialog_in_bounds)
      << "VerifyUi(): Dialog bounds " << dialog_bounds.ToString()
      << " outside of display work area " << display_work_area.ToString();
  return dialog_in_bounds;
#else
  NOTIMPLEMENTED();
  return false;
#endif
}

void TestBrowserDialog::WaitForUserDismissal() {
#if BUILDFLAG(IS_MAC)
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

std::string TestBrowserDialog::GetNonDialogName() {
  return std::string();
}

void TestBrowserDialog::UpdateWidgets() {
  widgets_.clear();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  for (aura::Window* root_window : ash::Shell::GetAllRootWindows())
    views::Widget::GetAllChildWidgets(root_window, &widgets_);
#elif defined(TOOLKIT_VIEWS)
  widgets_ = views::test::WidgetTest::GetAllWidgets();
#else
  NOTIMPLEMENTED();
#endif
}
