// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_H_

namespace gfx {
class Rect;
}

class PresentationReceiverWindowDelegate;

// This interface represents a window used to render a receiver page for the
// Presentation API.  It should display a WebContents provided by its controller
// and a location bar.  The window is not owned by the caller of Create (for
// Views, it is owned by the native widget and Views hierarchy).  It will call
// WindowClosing on |delegate| before being destroyed.
class PresentationReceiverWindow {
 public:
  static PresentationReceiverWindow* Create(
      PresentationReceiverWindowDelegate* delegate,
      const gfx::Rect& bounds);

  // Closes the window.
  virtual void Close() = 0;

  // Returns true if the window is active (i.e. has focus).
  virtual bool IsWindowActive() const = 0;

  // Returns true if the window is currently fullscreened.
  virtual bool IsWindowFullscreen() const = 0;

  // Exits fullscreen and enters windowed mode.
  virtual void ExitFullscreen() = 0;

  // Returns the current bounds of the window.
  virtual gfx::Rect GetWindowBounds() const = 0;

  // Shows the window as inactive and transitions it to fullscreen.
  virtual void ShowInactiveFullscreen() = 0;

  // Updates the window title if it has changed.
  virtual void UpdateWindowTitle() = 0;

  // Updates the location bar if the location of the displayed WebContents has
  // changed.
  virtual void UpdateLocationBar() = 0;

 protected:
  ~PresentationReceiverWindow() = default;
};

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_H_
