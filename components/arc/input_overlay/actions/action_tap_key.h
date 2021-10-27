// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_KEY_H_
#define COMPONENTS_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_KEY_H_

#include "components/arc/input_overlay/actions/action.h"

#include "ui/aura/window.h"

namespace arc {
namespace input_overlay {

// ActionTapKey transforms key event to touch event to simulate touch tap
// action.
class ActionTapKey : public Action {
 public:
  explicit ActionTapKey(aura::Window* window);
  ActionTapKey(const ActionTapKey&) = delete;
  ActionTapKey& operator=(const ActionTapKey&) = delete;
  ~ActionTapKey() override;

  // Override from Action.
  // Json value format:
  // {
  //   "name": "Fight",
  //   "key": "KeyA",
  //   "location": {
  //     "position": [...]
  //   }
  // }
  bool ParseFromJson(const base::Value& value) override;
  bool RewriteEvent(const ui::Event& origin,
                    std::list<ui::TouchEvent>& touch_events,
                    const gfx::RectF& content_bounds) override;
  void OnTouchCancelled() override;

  ui::DomCode key() { return key_; }

 private:
  bool RewriteKeyEvent(const ui::KeyEvent& key_event,
                       std::list<ui::TouchEvent>& rewritten_events,
                       const gfx::RectF& content_bounds);
  ui::DomCode key_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // COMPONENTS_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_KEY_H_
