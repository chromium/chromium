// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_

#include "content/public/browser/keyboard_event_processing_result.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

class BrowserFrame;
enum class TabDragKind;

namespace content {
struct NativeWebKeyboardEvent;
}

class NativeBrowserFrame {
 public:
  virtual ~NativeBrowserFrame() {}

  // Returns the platform specific InitParams for initializing our widget.
  virtual views::Widget::InitParams GetWidgetParams() = 0;

  // Returns |true| if we should use the custom frame.
  virtual bool UseCustomFrame() const = 0;

  // Returns true if the OS takes care of showing the system menu. Returning
  // false means BrowserFrame handles showing the system menu.
  virtual bool UsesNativeSystemMenu() const = 0;

  // Returns true when the window placement should be stored.
  virtual bool ShouldSaveWindowPlacement() const = 0;

  // Retrieves the window placement (show state and bounds) for restoring.
  virtual void GetWindowPlacement(gfx::Rect* bounds,
                                  ui::WindowShowState* show_state) const = 0;

  // Returns HANDLED if the |event| was handled by the platform implementation
  // before sending it to the renderer. E.g., it may be swallowed by a native
  // menu bar. Returns NOT_HANDLED_IS_SHORTCUT if the event was not handled, but
  // would be handled as a shortcut if the renderer chooses not to handle it.
  // Otherwise returns NOT_HANDLED.
  virtual content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;

  // Returns true if the |event| was handled by the platform implementation.
  virtual bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;

  // Returns true if there is a previously saved browser widget state that we
  // should restore. Returns false if we want to skip the given widget state
  // from browser restore, or use a widget state from a custom restore.
  virtual bool ShouldRestorePreviousBrowserWidgetState() const = 0;

  // Called when the tab drag kind for this frame changes.
  virtual void TabDraggingKindChanged(TabDragKind tab_drag_kind) {}

 protected:
  friend class BrowserFrame;

  // BrowserFrame pass-thrus ---------------------------------------------------
  // See browser_frame.h for documentation:
  virtual int GetMinimizeButtonOffset() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
