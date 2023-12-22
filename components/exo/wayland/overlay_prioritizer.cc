// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/overlay_prioritizer.h"

#include <overlay-prioritizer-server-protocol.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {
namespace {

// A property key containing a boolean set to true if a surface augmenter is
// associated with with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasOverlayPrioritizerKey, false)

////////////////////////////////////////////////////////////////////////////////
// overlay_prioritized_surface_interface:

// Implements the augmenter interface to a Surface. The "augmented"-state is set
// to null upon destruction. A window property will be set during the lifetime
// of this class to prevent multiple instances from being created for the same
// Surface.
class OverlayPrioritizedSurface : public SurfaceObserver {
 public:
  explicit OverlayPrioritizedSurface(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasOverlayPrioritizerKey, true);
  }
  OverlayPrioritizedSurface(const OverlayPrioritizedSurface&) = delete;
  OverlayPrioritizedSurface& operator=(const OverlayPrioritizedSurface&) =
      delete;
  ~OverlayPrioritizedSurface() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetProperty(kSurfaceHasOverlayPrioritizerKey, false);
    }
  }

  void SetOverlayPriority(uint priority) {
    OverlayPriority hint = OverlayPriority::REGULAR;
    switch (priority) {
      case OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_NONE:
        hint = OverlayPriority::LOW;
        break;
      case OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_REQUIRED_HARDWARE_PROTECTION:
      case OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_PREFERRED_LOW_LATENCY_CANVAS:
        hint = OverlayPriority::REQUIRED;
        break;
      default:
        hint = OverlayPriority::REGULAR;
    }
    surface_->SetOverlayPriorityHint(hint);
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  raw_ptr<Surface> surface_;
};

void overlay_prioritized_surface_destroy(wl_client* client,
                                         wl_resource* resource) {
  wl_resource_destroy(resource);
}

void overlay_prioritized_surface_set_overlay_priority(wl_client* client,
                                                      wl_resource* resource,
                                                      uint priority) {
  if (OVERLAY_PRIORITIZED_SURFACE_OVERLAY_PRIORITY_REQUIRED_HARDWARE_PROTECTION <
      priority) {
    wl_resource_post_error(resource,
                           OVERLAY_PRIORITIZED_SURFACE_ERROR_BAD_VALUE,
                           "priority is out of bound");
    return;
  }
  GetUserDataAs<OverlayPrioritizedSurface>(resource)->SetOverlayPriority(
      priority);
}

const struct overlay_prioritized_surface_interface prioritized_implementation =
    {overlay_prioritized_surface_destroy,
     overlay_prioritized_surface_set_overlay_priority};

////////////////////////////////////////////////////////////////////////////////
// overlay_prioritizer_interface:

void prioritizer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void prioritizer_get_prioritized_surface(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasOverlayPrioritizerKey)) {
    wl_resource_post_error(
        resource, OVERLAY_PRIORITIZER_ERROR_OVERLAY_HINTED_SURFACE_EXISTS,
        "a prioritizer for that surface already exists");
    return;
  }

  wl_resource* prioritized_resource =
      wl_resource_create(client, &overlay_prioritized_surface_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(prioritized_resource, &prioritized_implementation,
                    std::make_unique<OverlayPrioritizedSurface>(surface));
}

const struct overlay_prioritizer_interface prioritizer_implementation = {
    prioritizer_destroy, prioritizer_get_prioritized_surface};

}  // namespace

void bind_overlay_prioritizer(wl_client* client,
                              void* data,
                              uint32_t version,
                              uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &overlay_prioritizer_interface,
                         std::min(version, kOverlayPrioritizerVersion), id);

  wl_resource_set_implementation(resource, &prioritizer_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
