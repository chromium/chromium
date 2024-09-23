// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_

#include "build/build_config.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

class BrowserFrame;
enum class TabDragKind;

namespace input {
struct NativeWebKeyboardEvent;
}

class NativeBrowserFrame {
 public:
  virtual ~NativeBrowserFrame() = default;

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
  virtual void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const = 0;

  // Returns HANDLED if the |event| was handled by the platform implementation
  // before sending it to the renderer. E.g., it may be swallowed by a native
  // menu bar. Returns NOT_HANDLED_IS_SHORTCUT if the event was not handled, but
  // would be handled as a shortcut if the renderer chooses not to handle it.
  // Otherwise returns NOT_HANDLED.
  virtual content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Returns true if the |event| was handled by the platform implementation.
  virtual bool HandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Returns true if there is a previously saved browser widget state that we
  // should restore. Returns false if we want to skip the given widget state
  // from browser restore, or use a widget state from a custom restore.
  virtual bool ShouldRestorePreviousBrowserWidgetState() const = 0;

  // Returns true if the browser window should inherit the
  // `initial_visible_on_all_workspaces_state` of its previous browser window.
  // E.g. on ChromeOS it returns false when dragging a tab into a new
  // browser window so that the new window does not apply the initial
  // value so that new window inherits the current desk membership.
  // On the other OSes, it returns true to apply the initial value.
  virtual bool ShouldUseInitialVisibleOnAllWorkspaces() const = 0;

  // Called when the tab drag kind for this frame changes.
  virtual void TabDraggingKindChanged(TabDragKind tab_drag_kind) {}

#if BUILDFLAG(IS_MAC)
  // Causes the screen reader to announce |text| against the remote window. If
  // the current user is not using a screen reader or if there is no remote
  // window, has no effect. This enables screen reader announcements for
  // installed web apps (PWAs) on Mac. See crbug.com/1266922.
  virtual void AnnounceTextInInProcessWindow(const std::u16string& text) {}
#endif

 protected:
  friend class BrowserFrame;

  // BrowserFrame pass-thrus ---------------------------------------------------
  // See browser_frame.h for documentation:
  virtual int GetMinimizeButtonOffset() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
