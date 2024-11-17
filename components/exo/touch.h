// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TOUCH_H_
#define COMPONENTS_EXO_TOUCH_H_

#include "ash/shell_observer.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
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
class Touch : public ui::EventHandler,
              public SurfaceObserver,
              public ash::ShellObserver {
 public:
  Touch(TouchDelegate* delegate, Seat* seat);

  Touch(const Touch&) = delete;
  Touch& operator=(const Touch&) = delete;

  ~Touch() override;

  TouchDelegate* delegate() const { return delegate_; }

  // Set delegate for stylus events.
  void SetStylusDelegate(TouchStylusDelegate* delegate);
  bool HasStylusDelegate() const;

  // Overridden from ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // ash::ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnRootWindowWillShutdown(aura::Window* root_window) override;

 private:
  // Returns the effective target for |event|.
  Surface* GetEffectiveTargetForEvent(ui::LocatedEvent* event) const;

  // Cancels touches on all the surfaces.
  void CancelAllTouches();

  // The delegate instance that all events are dispatched to.
  const raw_ptr<TouchDelegate, DanglingUntriaged> delegate_;

  const raw_ptr<Seat> seat_;

  // The delegate instance that all stylus related events are dispatched to.
  raw_ptr<TouchStylusDelegate> stylus_delegate_ = nullptr;

  // Map of touch points to its focus surface.
  base::flat_map<int, raw_ptr<Surface, CtnExperimental>>
      touch_points_surface_map_;

  // Map of a touched surface to the count of touch pointers on that surface.
  base::flat_map<Surface*, int> surface_touch_count_map_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TOUCH_H_
