// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_POINTER_DELEGATE_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_POINTER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/exo/pointer_delegate.h"
#include "components/exo/wayland/wayland_input_delegate.h"

struct wl_client;
struct wl_resource;

namespace exo {
namespace wayland {
class SerialTracker;

// Pointer delegate class that accepts events for surfaces owned by the same
// client as a pointer resource.
class WaylandPointerDelegate : public WaylandInputDelegate,
                               public PointerDelegate {
 public:
  explicit WaylandPointerDelegate(wl_resource* pointer_resource,
                                  SerialTracker* serial_tracker);

  WaylandPointerDelegate(const WaylandPointerDelegate&) = delete;
  WaylandPointerDelegate& operator=(const WaylandPointerDelegate&) = delete;

  // Overridden from PointerDelegate:
  void OnPointerDestroying(Pointer* pointer) override;
  bool CanAcceptPointerEventsForSurface(Surface* surface) const override;
  void OnPointerEnter(Surface* surface,
                      const gfx::PointF& location,
                      int button_flags) override;
  void OnPointerLeave(Surface* surface) override;
  void OnPointerMotion(base::TimeTicks time_stamp,
                       const gfx::PointF& location) override;
  void OnPointerButton(base::TimeTicks time_stamp,
                       int button_flags,
                       bool pressed) override;
  void OnPointerScroll(base::TimeTicks time_stamp,
                       const gfx::Vector2dF& offset,
                       bool discrete) override;
  void OnFingerScrollStop(base::TimeTicks time_stamp) override;
  void OnPointerFrame() override;

 private:
  // The client who own this pointer instance.
  wl_client* client() const;

  // The pointer resource associated with the pointer.
  const raw_ptr<wl_resource> pointer_resource_;

  // Owned by Server, which always outlives this delegate.
  const raw_ptr<SerialTracker> serial_tracker_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_POINTER_DELEGATE_H_
