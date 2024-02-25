// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wp_viewporter.h"

#include <viewporter-server-protocol.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"
#include "ui/gfx/geometry/size_f.h"

namespace exo {
namespace wayland {
namespace {

// A property key containing a boolean set to true if a viewport is associated
// with with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasViewportKey, false)

////////////////////////////////////////////////////////////////////////////////
// wp_viewport_interface:

// Implements the viewport interface to a Surface. The "viewport"-state is set
// to null upon destruction. A window property will be set during the lifetime
// of this class to prevent multiple instances from being created for the same
// Surface.
class Viewport : public SurfaceObserver {
 public:
  explicit Viewport(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasViewportKey, true);
  }

  Viewport(const Viewport&) = delete;
  Viewport& operator=(const Viewport&) = delete;

  ~Viewport() override {
    if (surface_) {
      surface_->RemoveSurfaceObserver(this);
      surface_->SetCrop(gfx::RectF());
      surface_->SetViewport(gfx::SizeF());
      surface_->SetProperty(kSurfaceHasViewportKey, false);
    }
  }

  void SetSource(const gfx::RectF& rect) {
    if (surface_)
      surface_->SetCrop(rect);
  }

  void SetDestination(const gfx::Size& size) {
    if (surface_)
      surface_->SetViewport(gfx::SizeF(size));
  }

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  raw_ptr<Surface> surface_;
};

void viewport_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void viewport_set_source(wl_client* client,
                         wl_resource* resource,
                         wl_fixed_t x,
                         wl_fixed_t y,
                         wl_fixed_t width,
                         wl_fixed_t height) {
  if (x == wl_fixed_from_int(-1) && y == wl_fixed_from_int(-1) &&
      width == wl_fixed_from_int(-1) && height == wl_fixed_from_int(-1)) {
    GetUserDataAs<Viewport>(resource)->SetSource(gfx::RectF());
    return;
  }

  if (x < 0 || y < 0 || width <= 0 || height <= 0) {
    wl_resource_post_error(resource, WP_VIEWPORT_ERROR_BAD_VALUE,
                           "source rectangle must be non-empty (%dx%d) and"
                           "have positive origin (%d,%d)",
                           width, height, x, y);
    return;
  }

  GetUserDataAs<Viewport>(resource)->SetSource(
      gfx::RectF(wl_fixed_to_double(x), wl_fixed_to_double(y),
                 wl_fixed_to_double(width), wl_fixed_to_double(height)));
}

void viewport_set_destination(wl_client* client,
                              wl_resource* resource,
                              int32_t width,
                              int32_t height) {
  if (width == -1 && height == -1) {
    GetUserDataAs<Viewport>(resource)->SetDestination(gfx::Size());
    return;
  }

  if (width <= 0 || height <= 0) {
    wl_resource_post_error(resource, WP_VIEWPORT_ERROR_BAD_VALUE,
                           "destination size must be positive (%dx%d)", width,
                           height);
    return;
  }

  GetUserDataAs<Viewport>(resource)->SetDestination(gfx::Size(width, height));
}

const struct wp_viewport_interface viewport_implementation = {
    viewport_destroy, viewport_set_source, viewport_set_destination};

////////////////////////////////////////////////////////////////////////////////
// wp_viewporter_interface:

void viewporter_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void viewporter_get_viewport(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasViewportKey)) {
    wl_resource_post_error(resource, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS,
                           "a viewport for that surface already exists");
    return;
  }

  wl_resource* viewport_resource = wl_resource_create(
      client, &wp_viewport_interface, wl_resource_get_version(resource), id);

  SetImplementation(viewport_resource, &viewport_implementation,
                    std::make_unique<Viewport>(surface));
}

const struct wp_viewporter_interface viewporter_implementation = {
    viewporter_destroy, viewporter_get_viewport};

}  // namespace

void bind_viewporter(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wp_viewporter_interface, 1, id);

  wl_resource_set_implementation(resource, &viewporter_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
