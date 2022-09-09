// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_NATIVE_WINDOW_TRACKER_H_
#define CHROME_BROWSER_UI_NATIVE_WINDOW_TRACKER_H_

#include <memory>

#include "ui/gfx/native_widget_types.h"

// An observer which detects when a gfx::NativeWindow is closed.
class NativeWindowTracker {
 public:
  virtual ~NativeWindowTracker() {}

  static std::unique_ptr<NativeWindowTracker> Create(gfx::NativeWindow window);

  // Returns true if the native window passed to Create() has been closed.
  virtual bool WasNativeWindowClosed() const = 0;
};

#endif  // CHROME_BROWSER_UI_NATIVE_WINDOW_TRACKER_H_
