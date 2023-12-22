// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wp_fractional_scale.h"

#include "base/memory/raw_ptr.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"

namespace exo::wayland {
namespace {

// A property key containing a boolean set to true if a fractional scale object
// is associated with with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasFractionalScaleKey, false)

////////////////////////////////////////////////////////////////////////////////
// wp_fractional_scale_v1_interface:

// Implements the fractional scale interface to a Surface. The
// "fractional-scale"-state is set to null upon destruction. A window property
// will be set during the lifetime of this class to prevent multiple instances
// from being created for the same Surface.
class FractionalScale : public SurfaceObserver {
 public:
  explicit FractionalScale(Surface* surface, wl_resource* resource)
      : surface_(surface), resource_(resource) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasFractionalScaleKey, true);
    SendPreferredScale(0.0, surface_->GetDisplay().device_scale_factor());
  }
  FractionalScale(const FractionalScale&) = delete;
  FractionalScale& operator=(const FractionalScale&) = delete;
  ~FractionalScale() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetProperty(kSurfaceHasFractionalScaleKey, false);
    }
  }

  void SendPreferredScale(float old_scale_factor, float new_scale_factor) {
    uint32_t old_wire_scale = round(old_scale_factor * 120);
    uint32_t new_wire_scale = round(new_scale_factor * 120);
    DCHECK(new_wire_scale > 0);

    if (new_wire_scale != old_wire_scale) {
      wp_fractional_scale_v1_send_preferred_scale(resource_, new_wire_scale);
    }
  }

  void OnScaleFactorChanged(Surface* surface,
                            float old_scale_factor,
                            float new_scale_factor) override {
    SendPreferredScale(old_scale_factor, new_scale_factor);
  }

  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  raw_ptr<Surface> surface_;
  const raw_ptr<wl_resource> resource_;
};

void wp_fractional_scale_destroy(struct wl_client* client,
                                 struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wp_fractional_scale_v1_interface fractional_scale_implementation =
    {
        wp_fractional_scale_destroy,
};

////////////////////////////////////////////////////////////////////////////////
// wp_fractional_scale_manager_v1_interface:

void wp_fractional_scale_manager_destroy(struct wl_client* client,
                                         struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void wp_fractional_scale_manager_get_fractional_scale(
    wl_client* client,
    wl_resource* resource,
    uint32_t id,
    wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasFractionalScaleKey)) {
    wl_resource_post_error(
        resource, WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS,
        "a fractional scale object for that surface already exists");
    return;
  }

  wl_resource* fractional_scale_resource =
      wl_resource_create(client, &wp_fractional_scale_v1_interface,
                         wl_resource_get_version(resource), id);
  auto fractional_scale =
      std::make_unique<FractionalScale>(surface, fractional_scale_resource);
  SetImplementation(fractional_scale_resource, &fractional_scale_implementation,
                    std::move(fractional_scale));
}

static const struct wp_fractional_scale_manager_v1_interface
    fractional_scale_manager_implementation = {
        wp_fractional_scale_manager_destroy,
        wp_fractional_scale_manager_get_fractional_scale,
};

}  // namespace

void bind_fractional_scale_manager(wl_client* client,
                                   void* data,
                                   uint32_t version,
                                   uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wp_fractional_scale_manager_v1_interface,
                         std::min(version, kFractionalScaleVersion), id);

  wl_resource_set_implementation(
      resource, &fractional_scale_manager_implementation, data, nullptr);
}

}  // namespace exo::wayland
