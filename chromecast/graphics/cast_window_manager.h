// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_WINDOW_MANAGER_H_
#define CHROMECAST_GRAPHICS_CAST_WINDOW_MANAGER_H_

#include <memory>
#include <vector>

#include "chromecast/ui/mojom/ui_service.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class Event;
}  // namespace ui

namespace chromecast {

class CastTouchActivityObserver;
class CastGestureHandler;

// Chromecast's window-manager interface.
// This declares the interface to add top-level windows to the Chromecast
// platform window.  It is owned by the UI thread, and generally one instance
// should exist per platform root window (e.g., in Ozone, one per Ozone window).
class CastWindowManager {
 public:
  // Note: these window IDs are ordered by z-order.
  enum WindowId {
    BOTTOM = -1,
    // Base layer for WebUiManager and apps and activities managed by it.
    APP = BOTTOM,
    // Apps running in this layer won't be managed by WebUiManager.
    UNMANAGED_APP = 0,
    DEBUG_OVERLAY = 1,
    INFO_OVERLAY = 2,
    SOFT_KEYBOARD = 3,
    VOLUME = 4,
    MEDIA_INFO = 5,
    SETTINGS = 6,
    BOOT_ANIMATION_OVERLAY = 7,
    TOP = BOOT_ANIMATION_OVERLAY,
  };

  class Observer {
   public:
    // Advertises a change in the current window ordering. Use
    // CastWindowManager::GetWindowOrder() to retrieve the window ordering.
    virtual void WindowOrderChanged() = 0;

   protected:
    virtual ~Observer() = default;
  };

  virtual ~CastWindowManager() {}

  // Remove all windows and release all graphics resources.
  // Can be called multiple times.
  virtual void TearDown() = 0;

  // Adds a window to the window manager.
  // This doesn't necessarily make the window visible.
  // If the window manager hasn't been initialized, this has the side effect of
  // causing it to initialize.
  virtual void AddWindow(gfx::NativeView window) = 0;

  // Sets the Z order for the window. This allows windows with the same parent
  // to stack in a well-defined order.
  virtual void SetZOrder(gfx::NativeView window, mojom::ZOrder z_order) = 0;

  // Return the root window that holds all top-level windows.
  virtual gfx::NativeView GetRootWindow() = 0;

  // Returns the current window ordering. The IDs are ordered by z-order, from
  // lowest (bottom) to highest (top). Only visible windows are included in the
  // list.
  virtual std::vector<WindowId> GetWindowOrder() = 0;

  // Inject a UI event into the Cast window.
  virtual void InjectEvent(ui::Event* event) = 0;

  // Observer methods:
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Register a new handler for system gesture events.
  virtual void AddGestureHandler(CastGestureHandler* handler) = 0;

  // Remove the registration of a system gesture events handler.
  virtual void RemoveGestureHandler(CastGestureHandler* handler) = 0;

  // Enable/disable the handling of all touch events.
  virtual void SetTouchInputDisabled(bool disabled) = 0;

  // Add an observer for when input events occur while touch input is disabled.
  virtual void AddTouchActivityObserver(
      CastTouchActivityObserver* observer) = 0;

  // Remove an observer for when input events occur while touch input is
  // disabled.
  virtual void RemoveTouchActivityObserver(
      CastTouchActivityObserver* observer) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_WINDOW_MANAGER_H_
