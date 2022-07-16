// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NATIVE_WINDOW_TRACKER_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_NATIVE_WINDOW_TRACKER_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/native_window_tracker.h"

@class BridgedNativeWindowTracker;

class NativeWindowTrackerCocoa : public NativeWindowTracker {
 public:
  explicit NativeWindowTrackerCocoa(gfx::NativeWindow window);

  NativeWindowTrackerCocoa(const NativeWindowTrackerCocoa&) = delete;
  NativeWindowTrackerCocoa& operator=(const NativeWindowTrackerCocoa&) = delete;

  ~NativeWindowTrackerCocoa() override;

  // NativeWindowTracker:
  bool WasNativeWindowClosed() const override;

 private:
  base::scoped_nsobject<BridgedNativeWindowTracker> bridge_;
};

#endif  // CHROME_BROWSER_UI_COCOA_NATIVE_WINDOW_TRACKER_COCOA_H_
