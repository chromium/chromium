// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_INTERACTIVE_TEST_UTILS_H_
#define CHROME_TEST_BASE_INTERACTIVE_TEST_UTILS_H_

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/view_ids.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget_observer.h"

namespace display {
class Screen;
}  // namespace display

#if defined(TOOLKIT_VIEWS)
namespace views {
class View;
class Widget;
}
#endif

namespace ui_test_utils {

// Use in browser interactive uitests to wait until a browser is set to active.
// To use, create and call WaitForActivation(). Since on some platforms, the
// active browser list kept in |BrowserList| is updated before the actual
// activation (see |BrowserView::Show()| for details), observe the widget
// directly and wait for it to actually get activated.
class BrowserActivationWaiter : public views::WidgetObserver {
 public:
  explicit BrowserActivationWaiter(const Browser* browser);
  BrowserActivationWaiter(const BrowserActivationWaiter&) = delete;
  BrowserActivationWaiter& operator=(const BrowserActivationWaiter&) = delete;
  ~BrowserActivationWaiter() override;

  // Runs a message loop until the widget associated to |browser| supplied to
  // the constructor is activated, or returns immediately if |browser| has
  // already active. Should only be called once.
  void WaitForActivation();

  // views::WidgetObserver
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

 private:
  bool observed_ = false;
  base::RunLoop run_loop_;
};

// Use in browser interactive uitests to wait until a browser is deactivated.
// To use, create and call WaitForDeactivation().
class BrowserDeactivationWaiter : public BrowserListObserver {
 public:
  explicit BrowserDeactivationWaiter(const Browser* browser);
  BrowserDeactivationWaiter(const BrowserDeactivationWaiter&) = delete;
  BrowserDeactivationWaiter& operator=(const BrowserDeactivationWaiter&) =
      delete;
  ~BrowserDeactivationWaiter() override;

  // Runs a message loop until the |browser_| supplied to the constructor is
  // deactivated, or returns immediately if |browser_| has already become
  // inactive.
  // Should only be called once.
  void WaitForDeactivation();

 private:
  // BrowserListObserver:
  void OnBrowserNoLongerActive(Browser* browser) override;

  const base::WeakPtr<const Browser> browser_;
  bool observed_ = false;
  base::RunLoop run_loop_;
};

// Brings the native window for |browser| to the foreground and waits until the
// browser is active.
[[nodiscard]] bool BringBrowserWindowToFront(const Browser* browser);

// Returns true if the View is focused.
bool IsViewFocused(const Browser* browser, ViewID vid);

// Simulates a mouse click on a View in the browser.
void ClickOnView(views::View* view);
void ClickOnView(const Browser* browser, ViewID vid);

// Makes focus shift to the given View without clicking it.
void FocusView(const Browser* browser, ViewID vid);

// A collection of utilities that are used from interactive_ui_tests. These are
// separated from ui_test_utils.h to ensure that browser_tests don't use them,
// since they depend on focus which isn't possible for sharded test.

// Hide a native window.
void HideNativeWindow(gfx::NativeWindow window);

// Show and focus a native window. Returns true on success.
[[nodiscard]] bool ShowAndFocusNativeWindow(gfx::NativeWindow window);

// Sends key press and release events to a `browser` or `window`. Waits until at
// least the key release (or key press, depending on `wait_for`) events have
// been dispatched, or the test times out. It's useful to wait for key press
// instead of key release when the target may be deleted in response to key
// press. This may wait for key release even if `wait_for` is `kKeyPress` on
// platforms where it's possible to confirm that key release has been dispatched
// on a deleted target. This uses `ui_controls::SendKeyPress`, see it for
// details. Returns true if the event was successfully dispatched.
[[nodiscard]] bool SendKeyPressSync(
    const Browser* browser,
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command,
    ui_controls::KeyEventType wait_for = ui_controls::kKeyRelease);
[[nodiscard]] bool SendKeyPressToWindowSync(
    const gfx::NativeWindow window,
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command,
    ui_controls::KeyEventType wait_for = ui_controls::kKeyRelease);

// Sends a move event blocking until received. Returns true if the event was
// successfully received. This uses ui_controls::SendMouse***NotifyWhenDone,
// see it for details.
[[nodiscard]] bool SendMouseMoveSync(
    const gfx::Point& location,
    gfx::NativeWindow window_hint = ui_controls::kNoWindowHint);
[[nodiscard]] bool SendMouseEventsSync(
    ui_controls::MouseButton type,
    int button_state,
    gfx::NativeWindow window_hint = ui_controls::kNoWindowHint);

// A combination of SendMouseMove to the middle of the view followed by
// SendMouseEvents. Only exposed for toolkit-views.
// Alternatives: ClickOnView() and ui::test::EventGenerator.
#if defined(TOOLKIT_VIEWS)
void MoveMouseToCenterAndPress(
    views::View* view,
    ui_controls::MouseButton button,
    int button_state,
    base::OnceClosure task,
    int accelerator_state = ui_controls::kNoAccelerator);

void MoveMouseToCenterWithOffsetAndPress(
    views::View* view,
    const gfx::Vector2d& offset,
    ui_controls::MouseButton button,
    int button_state,
    base::OnceClosure task,
    int accelerator_state = ui_controls::kNoAccelerator);

// Returns the center of |view| in screen coordinates.
gfx::Point GetCenterInScreenCoordinates(const views::View* view);

// Blocks until the given view is focused (or not focused, depending on
// |focused|). Returns immediately if the state is already correct.
void WaitForViewFocus(Browser* browser, ViewID vid, bool focused);
void WaitForViewFocus(Browser* browser, views::View* view, bool focused);
#endif

#if BUILDFLAG(IS_MAC)
// Clear pressed modifier keys and report true if any key modifiers were down.
bool ClearKeyEventModifiers();
#endif

namespace internal {

// A utility function to send a mouse click event in a closure. It's shared by
// ui_controls_linux.cc and ui_controls_mac.cc
void ClickTask(ui_controls::MouseButton button,
               int button_state,
               base::OnceClosure followup,
               int accelerator_state = ui_controls::kNoAccelerator);

}  // namespace internal

// Returns the secondary display from the screen. DCHECKs if there is no such
// display.
display::Display GetSecondaryDisplay(display::Screen* screen);

// Returns the pair of displays -- the first one is the primary display and the
// second one is the other display.
std::pair<display::Display, display::Display> GetDisplays(
    display::Screen* screen);

}  // namespace ui_test_utils

#endif  // CHROME_TEST_BASE_INTERACTIVE_TEST_UTILS_H_
