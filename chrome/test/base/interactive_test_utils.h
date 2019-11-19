// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_INTERACTIVE_TEST_UTILS_H_
#define CHROME_TEST_BASE_INTERACTIVE_TEST_UTILS_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/view_ids.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"

namespace display {
class Screen;
}  // namespace display

#if defined(TOOLKIT_VIEWS)
namespace views {
class View;
}
#endif

namespace ui_test_utils {

// Use in browser interactive uitests to wait until a browser is set to active.
// To use, create and call WaitForActivation().
class BrowserActivationWaiter : public BrowserListObserver {
 public:
  explicit BrowserActivationWaiter(const Browser* browser);
  ~BrowserActivationWaiter() override;

  // Runs a message loop until the |browser_| supplied to the constructor is
  // activated, or returns immediately if |browser_| has already become active.
  // Should only be called once.
  void WaitForActivation();

 private:
  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  const Browser* const browser_;
  bool observed_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActivationWaiter);
};

// Use in browser interactive uitests to wait until a browser is deactivated.
// To use, create and call WaitForDeactivation().
class BrowserDeactivationWaiter : public BrowserListObserver {
 public:
  explicit BrowserDeactivationWaiter(const Browser* browser);
  ~BrowserDeactivationWaiter() override;

  // Runs a message loop until the |browser_| supplied to the constructor is
  // deactivated, or returns immediately if |browser_| has already become
  // inactive.
  // Should only be called once.
  void WaitForDeactivation();

 private:
  // BrowserListObserver:
  void OnBrowserNoLongerActive(Browser* browser) override;

  const Browser* const browser_;
  bool observed_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDeactivationWaiter);
};

// Brings the native window for |browser| to the foreground and waits until the
// browser is active.
bool BringBrowserWindowToFront(const Browser* browser) WARN_UNUSED_RESULT;

// Returns true if the View is focused.
bool IsViewFocused(const Browser* browser, ViewID vid);

// Simulates a mouse click on a View in the browser.
void ClickOnView(const Browser* browser, ViewID vid);

// Makes focus shift to the given View without clicking it.
void FocusView(const Browser* browser, ViewID vid);

// A collection of utilities that are used from interactive_ui_tests. These are
// separated from ui_test_utils.h to ensure that browser_tests don't use them,
// since they depend on focus which isn't possible for sharded test.

// Hide a native window.
void HideNativeWindow(gfx::NativeWindow window);

// Show and focus a native window. Returns true on success.
bool ShowAndFocusNativeWindow(gfx::NativeWindow window) WARN_UNUSED_RESULT;

// Sends a key press, blocking until the key press is received or the test times
// out. This uses ui_controls::SendKeyPress, see it for details. Returns true
// if the event was successfully sent and received.
bool SendKeyPressSync(const Browser* browser,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) WARN_UNUSED_RESULT;

// Sends a key press, blocking until the key press is received or the test times
// out. This uses ui_controls::SendKeyPress, see it for details. Returns true
// if the event was successfully sent and received.
bool SendKeyPressToWindowSync(const gfx::NativeWindow window,
                              ui::KeyboardCode key,
                              bool control,
                              bool shift,
                              bool alt,
                              bool command) WARN_UNUSED_RESULT;

// Sends a move event blocking until received. Returns true if the event was
// successfully received. This uses ui_controls::SendMouse***NotifyWhenDone,
// see it for details.
bool SendMouseMoveSync(const gfx::Point& location) WARN_UNUSED_RESULT;
bool SendMouseEventsSync(ui_controls::MouseButton type,
                         int button_state) WARN_UNUSED_RESULT;

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

// Returns the center of |view| in screen coordinates.
gfx::Point GetCenterInScreenCoordinates(const views::View* view);

// Blocks until the given view is focused (or not focused, depending on
// |focused|). Returns immediately if the state is already correct.
void WaitForViewFocus(Browser* browser, ViewID vid, bool focused);
#endif

#if defined(OS_MACOSX)
// Send press and release events for |key_code| with selected modifiers and wait
// until the last event arrives to our NSApp. Events will be sent as CGEvents
// through HID event tap. |key_code| must be a virtual key code (reference can
// be found in HIToolbox/Events.h from macOS SDK). |modifier_flags| must be a
// bitmask from ui::EventFlags.
void SendGlobalKeyEventsAndWait(int key_code, int modifier_flags);

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
