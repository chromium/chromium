// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui_test_utils {

namespace {

bool GetNativeWindow(const Browser* browser, gfx::NativeWindow* native_window) {
  BrowserWindow* window = browser->window();
  if (!window) {
    return false;
  }

  *native_window = window->GetNativeWindow();
  return !!(*native_window);
}

}  // namespace

BrowserActivationWaiter::BrowserActivationWaiter(const Browser* browser) {
  // When the active browser closes, the next "last active browser" in the
  // BrowserList might not be immediately activated. So we need to wait for the
  // "last active browser" to actually be active.
  if (chrome::FindLastActive() == browser && browser->window()->IsActive()) {
    observed_ = true;
    return;
  }

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  browser_view->frame()->AddObserver(this);
}

BrowserActivationWaiter::~BrowserActivationWaiter() = default;

void BrowserActivationWaiter::WaitForActivation() {
  if (observed_) {
    return;
  }
  DCHECK(!run_loop_.running()) << "WaitForActivation() can be called at most "
                                  "once. Construct a new "
                                  "BrowserActivationWaiter instead.";
  run_loop_.Run();
}

void BrowserActivationWaiter::OnWidgetActivationChanged(views::Widget* widget,
                                                        bool active) {
  if (!active) {
    return;
  }

  observed_ = true;
  widget->RemoveObserver(this);
  if (run_loop_.running()) {
    run_loop_.Quit();
  }
}

BrowserDeactivationWaiter::BrowserDeactivationWaiter(const Browser* browser)
    : browser_(browser->AsWeakPtr()) {
  if (chrome::FindLastActive() != browser && !browser->window()->IsActive()) {
    observed_ = true;
    return;
  }
  BrowserList::AddObserver(this);
}

BrowserDeactivationWaiter::~BrowserDeactivationWaiter() = default;

void BrowserDeactivationWaiter::WaitForDeactivation() {
  if (observed_) {
    return;
  }
  DCHECK(!run_loop_.running()) << "WaitForDeactivation() can be called at most "
                                  "once. Construct a new "
                                  "BrowserDeactivationWaiter instead.";
  run_loop_.Run();
}

void BrowserDeactivationWaiter::OnBrowserNoLongerActive(Browser* browser) {
  if (browser != browser_.get()) {
    return;
  }

  observed_ = true;
  BrowserList::RemoveObserver(this);
  if (run_loop_.running()) {
    run_loop_.Quit();
  }
}

bool BringBrowserWindowToFront(const Browser* browser) {
  gfx::NativeWindow window = gfx::NativeWindow();
  if (!GetNativeWindow(browser, &window)) {
    return false;
  }

  if (!ShowAndFocusNativeWindow(window)) {
    return false;
  }

  BrowserActivationWaiter waiter(browser);
  waiter.WaitForActivation();
  return true;
}

bool SendKeyPressSync(const Browser* browser,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command,
                      ui_controls::KeyEventType wait_for) {
  gfx::NativeWindow window = gfx::NativeWindow();
  if (!GetNativeWindow(browser, &window)) {
    return false;
  }
  return SendKeyPressToWindowSync(window, key, control, shift, alt, command,
                                  wait_for);
}

bool SendKeyPressToWindowSync(const gfx::NativeWindow window,
                              ui::KeyboardCode key,
                              bool control,
                              bool shift,
                              bool alt,
                              bool command,
                              ui_controls::KeyEventType wait_for) {
  CHECK(wait_for == ui_controls::KeyEventType::kKeyPress ||
        wait_for == ui_controls::KeyEventType::kKeyRelease);
#if BUILDFLAG(IS_WIN)
  DCHECK(key != ui::VKEY_ESCAPE || !control)
      << "'ctrl + esc' opens start menu on Windows. Start menu on windows "
         "2012 is a full-screen always on top window. It breaks all "
         "interactive tests.";
#endif

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool result = ui_controls::SendKeyPressNotifyWhenDone(
      window, key, control, shift, alt, command, run_loop.QuitClosure(),
      wait_for);
#if BUILDFLAG(IS_WIN)
  if (!result && ui_test_utils::ShowAndFocusNativeWindow(window)) {
    result = ui_controls::SendKeyPressNotifyWhenDone(
        window, key, control, shift, alt, command, run_loop.QuitClosure(),
        wait_for);
  }
#endif
  if (!result) {
    LOG(ERROR) << "ui_controls::SendKeyPressNotifyWhenDone failed";
    return false;
  }

  // Run the message loop. It'll stop running when either the key was received
  // or the test timed out (in which case testing::Test::HasFatalFailure should
  // be set).
  run_loop.Run();

  return !testing::Test::HasFatalFailure();
}

bool SendMouseMoveSync(const gfx::Point& location,
                       gfx::NativeWindow window_hint) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  if (!ui_controls::SendMouseMoveNotifyWhenDone(
          location.x(), location.y(), runner->QuitClosure(), window_hint)) {
    return false;
  }
  runner->Run();
  return !testing::Test::HasFatalFailure();
}

bool SendMouseEventsSync(ui_controls::MouseButton type,
                         int button_state,
                         gfx::NativeWindow window_hint) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  if (!ui_controls::SendMouseEventsNotifyWhenDone(
          type, button_state, runner->QuitClosure(),
          ui_controls::kNoAccelerator, window_hint)) {
    return false;
  }
  runner->Run();
  return !testing::Test::HasFatalFailure();
}

namespace internal {

void ClickTask(ui_controls::MouseButton button,
               int button_state,
               base::OnceClosure followup,
               int accelerator_state) {
  if (!followup.is_null()) {
    ui_controls::SendMouseEventsNotifyWhenDone(
        button, button_state, std::move(followup), accelerator_state);
  } else {
    ui_controls::SendMouseEvents(button, button_state, accelerator_state);
  }
}

}  // namespace internal

display::Display GetSecondaryDisplay(display::Screen* screen) {
  for (const auto& iter : screen->GetAllDisplays()) {
    if (iter.id() != screen->GetPrimaryDisplay().id()) {
      return iter;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return display::Display();
}

std::pair<display::Display, display::Display> GetDisplays(
    display::Screen* screen) {
  return std::make_pair(screen->GetPrimaryDisplay(),
                        GetSecondaryDisplay(screen));
}

}  // namespace ui_test_utils
