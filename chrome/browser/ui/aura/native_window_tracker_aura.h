// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_NATIVE_WINDOW_TRACKER_AURA_H_
#define CHROME_BROWSER_UI_AURA_NATIVE_WINDOW_TRACKER_AURA_H_

#include "base/macros.h"
#include "chrome/browser/ui/native_window_tracker.h"
#include "ui/aura/window_observer.h"

class NativeWindowTrackerAura : public NativeWindowTracker,
                                public aura::WindowObserver {
 public:
  explicit NativeWindowTrackerAura(gfx::NativeWindow window);
  ~NativeWindowTrackerAura() override;

  // NativeWindowTracker:
  bool WasNativeWindowClosed() const override;

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowTrackerAura);
};

#endif  // CHROME_BROWSER_UI_AURA_NATIVE_WINDOW_TRACKER_AURA_H_
