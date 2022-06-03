// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_ROUNDED_WINDOW_CORNERS_MANAGER_H_
#define CHROMECAST_GRAPHICS_ROUNDED_WINDOW_CORNERS_MANAGER_H_

#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace chromecast {

class CastWindowManager;
class RoundedCornersObserver;

// Manages rounded corner state, removing them when the topmost visible window
// is able to supply its own.
class RoundedWindowCornersManager : public aura::EnvObserver,
                                    public aura::WindowObserver {
 public:
  explicit RoundedWindowCornersManager(CastWindowManager* cast_window_manager);

  RoundedWindowCornersManager(const RoundedWindowCornersManager&) = delete;
  RoundedWindowCornersManager& operator=(const RoundedWindowCornersManager&) =
      delete;

  ~RoundedWindowCornersManager() override;

 private:
  void OnWindowInitialized(aura::Window* window) override;

  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  std::unique_ptr<RoundedCornersObserver> rounded_corners_observer_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ROUNDED_WINDOW_CORNERS_MANAGER_H_
