// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/surface_augmenter.h"

#include <surface-augmenter-server-protocol.h>

#include <memory>

#include "components/exo/buffer.h"
#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"

namespace exo {
namespace wayland {
namespace {

// A property key containing a boolean set to true if a surface augmenter is
// associated with with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasAugmentedSurfaceKey, false)

////////////////////////////////////////////////////////////////////////////////
// augmented_surface_interface:

// Implements the augmenter interface to a Surface. The "augmented"-state is set
// to null upon destruction. A window property will be set during the lifetime
// of this class to prevent multiple instances from being created for the same
// Surface.
class AugmentedSurface : public SurfaceObserver {
 public:
  explicit AugmentedSurface(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasAugmentedSurfaceKey, true);
  }
  AugmentedSurface(const AugmentedSurface&) = delete;
  AugmentedSurface& operator=(const AugmentedSurface&) = delete;
  ~AugmentedSurface() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetProperty(kSurfaceHasAugmentedSurfaceKey, false);
    }
  }

  void SetCorners(double top_left,
                  double top_right,
                  double bottom_right,
                  double bottom_left) {
    surface_->SetRoundedCorners(
        gfx::RoundedCornersF(top_left, top_right, bottom_right, bottom_left));
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  Surface* surface_;
};

void augmented_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void augmented_surface_set_corners(wl_client* client,
                                   wl_resource* resource,
                                   wl_fixed_t top_left,
                                   wl_fixed_t top_right,
                                   wl_fixed_t bottom_right,
                                   wl_fixed_t bottom_left) {
  if (top_left < 0 || bottom_left < 0 || bottom_right < 0 || top_right < 0) {
    wl_resource_post_error(
        resource, AUGMENTED_SURFACE_ERROR_BAD_VALUE,
        "All corner must have positive radius (%d, %d, %d, %d)", top_left,
        top_right, bottom_right, bottom_left);
    return;
  }

  GetUserDataAs<AugmentedSurface>(resource)->SetCorners(
      wl_fixed_to_double(top_left), wl_fixed_to_double(top_right),
      wl_fixed_to_double(bottom_right), wl_fixed_to_double(bottom_left));
}

const struct augmented_surface_interface augmented_implementation = {
    augmented_surface_destroy, augmented_surface_set_corners};

////////////////////////////////////////////////////////////////////////////////
// wl_buffer_interface:

void buffer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_buffer_interface buffer_implementation = {buffer_destroy};

////////////////////////////////////////////////////////////////////////////////
// surface_augmenter_interface:

void augmenter_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void HandleBufferReleaseCallback(wl_resource* resource) {
  wl_buffer_send_release(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void augmenter_create_solid_color_buffer(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t id,
                                         wl_array* color_data,
                                         int width,
                                         int height) {
  float* data = reinterpret_cast<float*>(color_data->data);
  SkColor4f color = {data[0], data[1], data[2], data[3]};
  std::unique_ptr<SolidColorBuffer> buffer =
      std::make_unique<SolidColorBuffer>(color, gfx::Size(width, height));
  wl_resource* buffer_resource = wl_resource_create(
      client, &wl_buffer_interface, wl_resource_get_version(resource), id);

  buffer->set_release_callback(base::BindRepeating(
      &HandleBufferReleaseCallback, base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));
}

void augmenter_get_augmented_surface(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasAugmentedSurfaceKey)) {
    wl_resource_post_error(resource,
                           SURFACE_AUGMENTER_ERROR_AUGMENTED_SURFACE_EXISTS,
                           "an augmenter for that surface already exists");
    return;
  }

  wl_resource* augmented_resource =
      wl_resource_create(client, &augmented_surface_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(augmented_resource, &augmented_implementation,
                    std::make_unique<AugmentedSurface>(surface));
}

const struct surface_augmenter_interface augmenter_implementation = {
    augmenter_destroy, augmenter_create_solid_color_buffer,
    augmenter_get_augmented_surface};

}  // namespace

void bind_surface_augmenter(wl_client* client,
                            void* data,
                            uint32_t version,
                            uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &surface_augmenter_interface,
                         std::min(version, kSurfaceAugmenterVersion), id);

  wl_resource_set_implementation(resource, &augmenter_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
