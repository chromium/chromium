// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_NATIVE_WINDOW_TRACKER_AURA_H_
#define CHROME_BROWSER_UI_AURA_NATIVE_WINDOW_TRACKER_AURA_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/native_window_tracker.h"
#include "ui/aura/window_observer.h"

class NativeWindowTrackerAura : public NativeWindowTracker,
                                public aura::WindowObserver {
 public:
  explicit NativeWindowTrackerAura(gfx::NativeWindow window);

  NativeWindowTrackerAura(const NativeWindowTrackerAura&) = delete;
  NativeWindowTrackerAura& operator=(const NativeWindowTrackerAura&) = delete;

  ~NativeWindowTrackerAura() override;

  // NativeWindowTracker:
  bool WasNativeWindowClosed() const override;

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  raw_ptr<aura::Window> window_;
};

#endif  // CHROME_BROWSER_UI_AURA_NATIVE_WINDOW_TRACKER_AURA_H_
