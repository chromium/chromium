// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_CAST_TOUCH_EVENT_GATE_H_
#define CHROMECAST_GRAPHICS_CAST_TOUCH_EVENT_GATE_H_

#include "base/containers/flat_set.h"
#include "ui/events/event_rewriter.h"

namespace aura {
class Window;
}  // namespace aura

namespace chromecast {

class CastTouchActivityObserver;

// An event rewriter whose purpose is to discard events (when enabled).  This
// class is meant to be installed as the first rewriter in the chain, to handle
// scenarios where all input needs to be disabled, such as when the device
// screen is turned off.  Instances of CastTouchActivityObserver can be
// registered to receive notifications of gated events.
class CastTouchEventGate : public ui::EventRewriter {
 public:
  explicit CastTouchEventGate(aura::Window* root_window);
  ~CastTouchEventGate() override;

  void SetEnabled(bool enabled);

  void AddObserver(CastTouchActivityObserver* observer);
  void RemoveObserver(CastTouchActivityObserver* observer);

  // ui::EventRewriter implementation.
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  bool enabled_ = false;
  aura::Window* root_window_;
  base::flat_set<CastTouchActivityObserver*> observers_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_CAST_TOUCH_EVENT_GATE_H_
