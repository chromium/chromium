// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
#define COMPONENTS_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_

#include "base/scoped_observation.h"
#include "components/arc/input_overlay/actions/action.h"
#include "ui/aura/window.h"

namespace arc {
// TouchInjector includes all the touch actions related to the specific window
// and performs as a bridge between the ArcInputOverlayManager and the touch
// actions. It implements EventRewriter to transform input events to touch
// events.
class TouchInjector : public ui::EventRewriter {
 public:
  explicit TouchInjector(aura::Window* top_level_window);
  TouchInjector(const TouchInjector&) = delete;
  TouchInjector& operator=(const TouchInjector&) = delete;
  ~TouchInjector() override;

  aura::Window* target_window() { return target_window_; }
  const std::vector<std::unique_ptr<input_overlay::Action>>& actions() const {
    return actions_;
  }

  // Parse Json to actions.
  // Json value format:
  // {
  //   "tap": {
  //     "keyboard": [],
  //     "mouse": []
  //   },
  //   "move": {
  //     "keyboard": [],
  //     "mouse": []
  //   },
  //   ...
  // }
  void ParseActions(const base::Value& root);
  // Notify the EventRewriter whether the text input is focused or not.
  void NotifyTextInputState(bool active);
  // Register the EventRewriter.
  void RegisterEventRewriter();
  // Unregister the EventRewriter.
  void UnRegisterEventRewriter();
  // If the window is destroying or focusing out, releasing the active touch
  // event.
  void DispatchTouchCancelEvent();

  // Overridden from ui::EventRewriter
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  aura::Window* target_window_;
  base::WeakPtr<ui::EventRewriterContinuation> continuation_;
  std::vector<std::unique_ptr<input_overlay::Action>> actions_;
  base::ScopedObservation<ui::EventSource,
                          ui::EventRewriter,
                          &ui::EventSource::AddEventRewriter,
                          &ui::EventSource::RemoveEventRewriter>
      observation_{this};
  bool text_input_active_ = false;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_H_
