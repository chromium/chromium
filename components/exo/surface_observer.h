// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_OBSERVER_H_
#define COMPONENTS_EXO_SURFACE_OBSERVER_H_

#include <cstdint>
#include <string>

namespace gfx {
class Rect;
}

namespace exo {
class Surface;
enum class OverlayPriority;

// Observers can listen to various events on the Surfaces.
class SurfaceObserver {
 public:
  // Called at the top of the surface's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnSurfaceDestroying(Surface* surface) = 0;

  virtual void OnScaleFactorChanged(Surface* surface,
                                    float old_scale_factor,
                                    float new_scale_factor) {}

  // Called when the occlusion of the aura window corresponding to |surface|
  // changes.
  virtual void OnWindowOcclusionChanged(Surface* surface) {}

  // Called when frame is locked to normal state or unlocked from
  // previously locked state.
  virtual void OnFrameLockingChanged(Surface* surface, bool lock) {}

  // Called on each commit.
  virtual void OnCommit(Surface* surface) {}

  // Called when desk state of the window changes.
  // |state| is the index of the desk which the window moved to,
  // or -1 for a window assigned to all desks.
  virtual void OnDeskChanged(Surface* surface, int state) {}

  // Called when the display of this surface has changed. Only called after
  // successfully updating sub-surfaces.
  virtual void OnDisplayChanged(Surface* surface,
                                int64_t old_display,
                                int64_t new_display) {}

  // Starts or ends throttling.
  virtual void ThrottleFrameRate(bool on) {}

  // Called when tooltip is shown.
  // `bounds` is relative to `surface`.
  virtual void OnTooltipShown(Surface* surface,
                              const std::u16string& text,
                              const gfx::Rect& bounds) {}

  // Called when tooltip is hidden.
  virtual void OnTooltipHidden(Surface* surface) {}

  virtual void OnFullscreenStateChanged(bool fullscreen) {}

  virtual void OnOverlayPriorityHintChanged(
      OverlayPriority overlay_priority_hint) {}

 protected:
  virtual ~SurfaceObserver() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_OBSERVER_H_
