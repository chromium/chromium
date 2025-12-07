// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_INTERACTIVE_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_INTERACTIVE_TEST_MIXIN_H_

#include <concepts>

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/interactive_test.h"

enum class InputSource { INPUT_SOURCE_MOUSE = 0, INPUT_SOURCE_TOUCH = 1 };

// Template to be used as a mixin class for tab dragging tests extending
// InProcessBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class TabDragControllerInteractiveTestMixin : public T {
 public:
  template <class... Args>
  explicit TabDragControllerInteractiveTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~TabDragControllerInteractiveTestMixin() override = default;
  TabDragControllerInteractiveTestMixin(
      const TabDragControllerInteractiveTestMixin&) = delete;
  TabDragControllerInteractiveTestMixin& operator=(
      const TabDragControllerInteractiveTestMixin&) = delete;

  virtual InputSource input_source() { return InputSource::INPUT_SOURCE_MOUSE; }

#if BUILDFLAG(IS_CHROMEOS)
  bool SendTouchEventsSync(int action, int id, const gfx::Point& location) {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    if (!ui_controls::SendTouchEventsNotifyWhenDone(
            action, id, location.x(), location.y(), run_loop.QuitClosure())) {
      return false;
    }
    run_loop.Run();
    return true;
  }
#endif

  gfx::NativeWindow GetWindowHint(const views::View* view) {
    return view->GetWidget() ? view->GetWidget()->GetNativeWindow()
                             : gfx::NativeWindow();
  }

  // The following methods update one of the mouse or touch input depending upon
  // the InputSource.
  bool PressInput(const gfx::Point& location,
                  const gfx::NativeWindow window_hint,
                  int id = 0) {
    if (input_source() == InputSource::INPUT_SOURCE_MOUSE) {
      return ui_test_utils::SendMouseMoveSync(location, window_hint) &&
             ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                                ui_controls::DOWN, window_hint);
    }
#if BUILDFLAG(IS_CHROMEOS)
    return SendTouchEventsSync(ui_controls::kTouchPress, id, location);
#else
    NOTREACHED();
#endif
  }

  // Like PressInput() used together with GetCenterInScreenCoordinates(), but
  // also automatically passes a window hint to the ui_test_utils functions
  // used. This is sometimes needed on Wayland to ensure the mouse events are
  // sent to the correct location.
  bool PressInputAtCenter(const views::View* view, int id = 0) {
    return PressInput(ui_test_utils::GetCenterInScreenCoordinates(view),
                      GetWindowHint(view), id);
  }

  bool DragInputTo(const gfx::Point& location, gfx::NativeWindow window_hint) {
    if (input_source() == InputSource::INPUT_SOURCE_MOUSE) {
      return ui_test_utils::SendMouseMoveSync(location, window_hint);
    }
#if BUILDFLAG(IS_CHROMEOS)
    return SendTouchEventsSync(ui_controls::kTouchMove, 0, location);
#else
    NOTREACHED();
#endif
  }

  // Like PressInputAtCenter(), but for DragInputTo() instead of PressInput()
  // and with an offset applied to the view's center.
  bool DragInputToCenter(const views::View* view, gfx::Vector2d offset = {}) {
    return DragInputTo(
        ui_test_utils::GetCenterInScreenCoordinates(view) + offset,
        GetWindowHint(view));
  }

  bool ReleaseInput(int id = 0, bool async = false) {
    if (input_source() == InputSource::INPUT_SOURCE_MOUSE) {
      return async ? ui_controls::SendMouseEvents(ui_controls::LEFT,
                                                  ui_controls::UP)
                   : ui_test_utils::SendMouseEventsSync(ui_controls::LEFT,
                                                        ui_controls::UP);
    }
#if BUILDFLAG(IS_CHROMEOS)
    return async ? ui_controls::SendTouchEvents(ui_controls::kTouchRelease, id,
                                                0, 0)
                 : SendTouchEventsSync(ui_controls::kTouchRelease, id,
                                       gfx::Point());
#else
    NOTREACHED();
#endif
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_CONTROLLER_INTERACTIVE_TEST_MIXIN_H_
