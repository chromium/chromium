// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INPUT_METHOD_IME_NATIVE_WINDOW_H_
#define CHROME_BROWSER_UI_INPUT_METHOD_IME_NATIVE_WINDOW_H_

namespace gfx {
class Rect;
}

namespace ui {

// The interface to bridge the interactions between ImeWindow and ImeWindowView.
// Note that c/b/ui cannot depend upon the platform specific implementations.
// This is held weakly by ImeWindow. The subclass should do self-destruction
// while the IME window (widget) is destroyed, and its destructor is expected
// to call to ImeWindow::OnWindowDestroyed().
class ImeNativeWindow {
 public:
  // Shows the native IME window.
  virtual void Show() = 0;

  // Hides the native IME window.
  virtual void Hide() = 0;

  // Hides the native IME window.
  virtual void Close() = 0;

  // Sets the bounds of the native window.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Gets the bounds of the native window.
  virtual gfx::Rect GetBounds() const = 0;

  // Updates the window's title icon.
  virtual void UpdateWindowIcon() = 0;

  // For testing.
  virtual bool IsVisible() const = 0;

 protected:
  virtual ~ImeNativeWindow() {}
};

}  // namespace ui

#endif  // CHROME_BROWSER_UI_INPUT_METHOD_IME_NATIVE_WINDOW_H_
