// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_H_
#define CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

class Browser;

namespace display {
class Display;
class Screen;
}

///////////////////////////////////////////////////////////////////////////////
// WindowSizer
//
//  A class that determines the best new size and position for a window to be
//  shown at based several factors, including the position and size of the last
//  window of the same type, the last saved bounds of the window from the
//  previous session, and default system metrics if neither of the above two
//  conditions exist. The system has built-in providers for monitor metrics
//  and persistent storage (using preferences) but can be overrided with mocks
//  for testing.
//
// TODO(crbug.com/846736): Extract the platform-specific code out of this class.
class WindowSizer {
 public:
  // An interface implemented by an object that can retrieve state from either a
  // persistent store or an existing window.
  class StateProvider {
   public:
    virtual ~StateProvider() = default;

    // Retrieve the persisted bounds of the window. Returns true if there were
    // persisted bounds and false otherwise. If this method returns false, none
    // of the out parameters are touched. If it returns true, |bounds| was
    // overwritten, and |work_area| may have been overwritten if there was also
    // a saved work area.  The |show_state| variable will only be touched if
    // there was persisted data and the |show_state| variable is
    // WindowShowState::kDefault.
    virtual bool GetPersistentState(
        gfx::Rect* bounds,
        gfx::Rect* work_area,
        ui::mojom::WindowShowState* show_state) const = 0;

    // Retrieve the bounds of the most recent window of the matching type.
    // Returns true if there was a last active window to retrieve state
    // information from, false otherwise.
    // The |show_state| variable will only be touched if we have found a
    // suitable window and the |show_state| variable is
    // WindowShowState::kDefault.
    virtual bool GetLastActiveWindowState(
        gfx::Rect* bounds,
        ui::mojom::WindowShowState* show_state) const = 0;
  };

  WindowSizer(const WindowSizer&) = delete;
  WindowSizer& operator=(const WindowSizer&) = delete;

  // Determines the position and size for a window as it is created as well
  // as the initial state. This function uses several strategies to figure out
  // optimal size and placement, first looking for an existing active window,
  // then falling back to persisted data from a previous session, finally
  // utilizing a default algorithm. If |specified_bounds| are non-empty, this
  // value is returned instead. To explicitly specify a particular window to
  // base the bounds on, pass in a non-null value for |browser|.
  //
  // |show_state| will be overwritten and return the initial visual state of
  // the window to use.
  static void GetBrowserWindowBoundsAndShowState(
      const gfx::Rect& specified_bounds,
      const Browser* browser,
      gfx::Rect* window_bounds,
      ui::mojom::WindowShowState* show_state);

  // As above, but takes a state provider for testing.
  static void GetBrowserWindowBoundsAndShowState(
      std::unique_ptr<StateProvider> state_provider,
      const gfx::Rect& specified_bounds,
      const Browser* browser,
      gfx::Rect* window_bounds,
      ui::mojom::WindowShowState* show_state);

  // Returns the default origin for popups of the given size.
  static gfx::Point GetDefaultPopupOrigin(const gfx::Size& size);

  // How much horizontal and vertical offset there is between newly
  // opened windows.  This value may be different on each platform.
  static const int kWindowTilePixels;

  // The maximum default window width. This value may differ between platforms.
  static const int kWindowMaxDefaultWidth;

 protected:
  const StateProvider* state_provider() const { return state_provider_.get(); }
  const Browser* browser() const { return browser_; }

  // WindowSizer will use the platform's display::Screen.
  WindowSizer(std::unique_ptr<StateProvider> state_provider,
              const Browser* browser);
  virtual ~WindowSizer();

  // See GetBrowserWindowBoundsAndShowState() above.
  virtual void DetermineWindowBoundsAndShowState(
      const gfx::Rect& specified_bounds,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state);

  // Adjusts the work area the platform-specific way.
  virtual void AdjustWorkAreaForPlatform(gfx::Rect& work_area);

  // Gets the size and placement of the last active window. Returns true if this
  // data is valid, false if there is no last window and the application should
  // restore saved state from preferences using RestoreWindowPosition.
  // |show_state| will only be changed if it was set to
  // WindowShowState::kDefault.
  bool GetLastActiveWindowBounds(gfx::Rect* bounds,
                                 ui::mojom::WindowShowState* show_state) const;

  // Gets the size and placement of the last window in the last session, saved
  // in local state preferences. Returns true if local state exists containing
  // this information, false if this information does not exist and a default
  // size should be used.
  // |show_state| will only be changed if it was set to
  // WindowShowState::kDefault.
  bool GetSavedWindowBounds(gfx::Rect* bounds,
                            ui::mojom::WindowShowState* show_state) const;

  // Gets the default window position and size to be shown on
  // |display| if there is no last window and no saved window
  // placement in prefs. This function determines the default size
  // based on monitor size, etc.
  virtual gfx::Rect GetDefaultWindowBounds(
      const display::Display& display) const;

  // Adjusts |bounds| to be visible on-screen, biased toward the work area of
  // the |display|.  Despite the name, this doesn't
  // guarantee the bounds are fully contained within this display's work rect;
  // it just tried to ensure the edges are visible on _some_ work rect.
  // If |saved_work_area| is non-empty, it is used to determine whether the
  // monitor configuration has changed. If it has, bounds are repositioned and
  // resized if necessary to make them completely contained in the current work
  // area.
  void AdjustBoundsToBeVisibleOnDisplay(const display::Display& display,
                                        const gfx::Rect& saved_work_area,
                                        gfx::Rect* bounds) const;

  // Determine the default show state for the window - not looking at other
  // windows or at persistent information.
  static ui::mojom::WindowShowState GetWindowDefaultShowState(
      const Browser* browser);

  // Returns the target display for a new window with |bounds| in screen
  // coordinates.
  static display::Display GetDisplayForNewWindow(
      const gfx::Rect& bounds = gfx::Rect());

 private:
  friend class WindowSizerTestUtil;

  // Providers for persistent storage and monitor metrics.
  std::unique_ptr<StateProvider> state_provider_;

  // Note that this browser handle might be NULL.
  const raw_ptr<const Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_H_
