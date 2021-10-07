// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/input_overlay/touch_injector.h"

#include "components/arc/input_overlay/resources/input_overlay_resources_util.h"

namespace arc {

TouchInjector::TouchInjector(aura::Window* top_level_window)
    : target_window_(top_level_window) {}

TouchInjector::~TouchInjector() = default;

void TouchInjector::ParseActions(const base::Value& root) {
  auto parsed_actions = ParseJsonToActions(target_window_, root);
  if (!parsed_actions)
    return;
  std::move(parsed_actions->begin(), parsed_actions->end(),
            std::back_inserter(actions_));
}

}  // namespace arc
