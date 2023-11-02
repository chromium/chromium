// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_GESTURES_MULTIPLE_TAP_DETECTOR_H_
#define CHROMECAST_GRAPHICS_GESTURES_MULTIPLE_TAP_DETECTOR_H_

#include <deque>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ui {
class Event;
class TouchEvent;
}  // namespace ui

namespace chromecast {

class MultipleTapDetectorDelegate {
 public:
  virtual ~MultipleTapDetectorDelegate() = default;
  virtual void OnTripleTap(const gfx::Point& touch_location) = 0;
  virtual void OnDoubleTap(const gfx::Point& touch_location) {}
};

enum class MultiTapState {
  NONE,
  TOUCH,
  INTERVAL_WAIT,
};

// An event rewriter responsible for detecting triple-tap or double-tap events
// on the root window.
class MultipleTapDetector : public ui::EventRewriter {
 public:
  MultipleTapDetector(aura::Window* root_window,
                      MultipleTapDetectorDelegate* delegate);

  MultipleTapDetector(const MultipleTapDetector&) = delete;
  MultipleTapDetector& operator=(const MultipleTapDetector&) = delete;

  ~MultipleTapDetector() override;

  void set_enabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }

  // Overridden from ui::EventRewriter
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

 private:
  friend class MultipleTapDetectorTest;

  // Expiration event for maximum time between taps in a tap.
  void OnTapIntervalTimerFired();
  // Expiration event for a finger that is pressed too long during a multi tap.
  void OnLongPressIntervalTimerFired();
  void TapDetectorStateReset();

  void DispatchEvent(ui::TouchEvent* event);

  // A default gesture detector config, so we can share the same
  // timeout and pixel slop constants.
  ui::GestureDetector::Config gesture_detector_config_;

  aura::Window* root_window_;
  MultipleTapDetectorDelegate* delegate_;

  bool enabled_;

  MultiTapState tap_state_;
  int tap_count_;
  gfx::Point last_tap_location_;
  base::OneShotTimer triple_tap_timer_;
  class Stash {
   public:
    Stash(const ui::TouchEvent& e, const Continuation c);
    ~Stash();
    const ui::TouchEvent event;
    const Continuation continuation;
  };
  std::deque<Stash> stashed_events_;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_GESTURES_MULTIPLE_TAP_DETECTOR_H_
