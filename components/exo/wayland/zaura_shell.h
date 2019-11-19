// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZAURA_SHELL_H_
#define COMPONENTS_EXO_WAYLAND_ZAURA_SHELL_H_

#include <stdint.h>

#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "ui/wm/public/activation_change_observer.h"

struct wl_client;
struct wl_resource;

namespace exo {
namespace wayland {

constexpr uint32_t kZAuraShellVersion = 9;

// Adds bindings to the Aura Shell. Normally this implies Ash on ChromeOS
// builds. On non-ChromeOS builds the protocol provides access to Aura windowing
// system.
void bind_aura_shell(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id);

class AuraSurface : public SurfaceObserver,
                    public ::wm::ActivationChangeObserver {
 public:
  AuraSurface(Surface* surface, wl_resource* resource);
  ~AuraSurface() override;

  void SetFrame(SurfaceFrameType type);
  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);
  void SetParent(AuraSurface* parent, const gfx::Point& position);
  void SetStartupId(const char* startup_id);
  void SetApplicationId(const char* application_id);
  void SetClientSurfaceId(int client_surface_id);
  void SetOcclusionTracking(bool tracking);
  void Activate();
  void DrawAttention();

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;
  void OnWindowOcclusionChanged(Surface* surface) override;

  // Overridden from ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gaining_active,
                          aura::Window* losing_active) override;
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {}

 protected:
  virtual void SendOcclusionFraction(float occlusion_fraction);

 private:
  Surface* surface_;
  wl_resource* const resource_;

  void ComputeAndSendOcclusionFraction(
      const aura::Window::OcclusionState occlusion_state,
      const SkRegion& occluded_region);

  DISALLOW_COPY_AND_ASSIGN(AuraSurface);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZAURA_SHELL_H_
