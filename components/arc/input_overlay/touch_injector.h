// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
#define COMPONENTS_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_

#include "components/arc/input_overlay/actions/action.h"
#include "ui/aura/window.h"

namespace arc {
// TouchInjector includes all the touch actions related to the specific window
// and performs as a bridge between the ArcInputOverlayManager and the touch
// actions.
class TouchInjector {
 public:
  explicit TouchInjector(aura::Window* top_level_window);
  TouchInjector(const TouchInjector&) = delete;
  TouchInjector& operator=(const TouchInjector&) = delete;
  ~TouchInjector();

  aura::Window* target_window() { return target_window_; }
  std::vector<std::unique_ptr<input_overlay::Action>>& actions() {
    return actions_;
  }

  void ParseActions(const base::Value& root);

 private:
  aura::Window* target_window_;
  std::vector<std::unique_ptr<input_overlay::Action>> actions_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
