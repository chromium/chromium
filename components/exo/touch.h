// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TOUCH_H_
#define COMPONENTS_EXO_TOUCH_H_

#include <vector>

#include "base/macros.h"
#include "components/exo/surface_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {
class LocatedEvent;
class TouchEvent;
}

namespace exo {
class Seat;
class TouchDelegate;
class TouchStylusDelegate;

// This class implements a client touch device that represents one or more
// touch devices.
class Touch : public ui::EventHandler, public SurfaceObserver {
 public:
  Touch(TouchDelegate* delegate, Seat* seat);
  ~Touch() override;

  TouchDelegate* delegate() const { return delegate_; }

  // Set delegate for stylus events.
  void SetStylusDelegate(TouchStylusDelegate* delegate);
  bool HasStylusDelegate() const;

  // Overridden from ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

 private:
  // Returns the effective target for |event|.
  Surface* GetEffectiveTargetForEvent(ui::LocatedEvent* event) const;

  // The delegate instance that all events are dispatched to.
  TouchDelegate* const delegate_;

  Seat* const seat_;

  // The delegate instance that all stylus related events are dispatched to.
  TouchStylusDelegate* stylus_delegate_ = nullptr;

  // The current focus surface for the touch device.
  Surface* focus_ = nullptr;

  // Vector of touch points in focus surface.
  std::vector<int> touch_points_;

  DISALLOW_COPY_AND_ASSIGN(Touch);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TOUCH_H_
