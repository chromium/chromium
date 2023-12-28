// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/content_type.h"

#include <content-type-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/memory/raw_ptr.h"
#include "components/exo/surface_observer.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {
namespace {

// A property key containing a boolean set to true if a content type
// is associated with with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasContentTypeKey, false)

// A wrapper around a surface that observes lifetime events and translates
// content types into surface properties.
class SurfaceContentType : SurfaceObserver {
 public:
  explicit SurfaceContentType(Surface* surface) : surface_(surface) {
    surface_->AddSurfaceObserver(this);
    surface_->SetProperty(kSurfaceHasContentTypeKey, true);
  }
  SurfaceContentType(const SurfaceContentType&) = delete;
  SurfaceContentType& operator=(const SurfaceContentType&) = delete;
  ~SurfaceContentType() override {
    if (surface_) {
      ResetSurface();
      surface_->RemoveSurfaceObserver(this);
      surface_->SetProperty(kSurfaceHasContentTypeKey, false);
    }
  }

  void UpdateContentType(uint32_t content_type) {
    if (!surface_)
      return;

    surface_->SetContainsVideo(content_type == WP_CONTENT_TYPE_V1_TYPE_VIDEO);
  }

  // SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override {
    surface_->RemoveSurfaceObserver(this);
    surface_ = nullptr;
  }

 private:
  void ResetSurface() {
    if (!surface_)
      return;

    surface_->SetContainsVideo(false);
  }

  raw_ptr<Surface> surface_;
};

////////////////////////////////////////////////////////////////////////////////
// wp_content_type_v1_interface:

void content_type_destroy(struct wl_client* client,
                          struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

void content_type_set_content_type(struct wl_client* client,
                                   struct wl_resource* resource,
                                   uint32_t content_type) {
  GetUserDataAs<SurfaceContentType>(resource)->UpdateContentType(content_type);
}

const struct wp_content_type_v1_interface content_type_interface = {
    .destroy = &content_type_destroy,
    .set_content_type = &content_type_set_content_type,
};

////////////////////////////////////////////////////////////////////////////////
// wp_content_type_manager_v1_interface:

void content_type_manager_destroy(struct wl_client* client,
                                  struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

void content_type_manger_get_surface_content_type(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t id,
    struct wl_resource* surface_resource) {
  Surface* surface = GetUserDataAs<Surface>(surface_resource);
  if (surface->GetProperty(kSurfaceHasContentTypeKey)) {
    wl_resource_post_error(
        resource, WP_CONTENT_TYPE_MANAGER_V1_ERROR_ALREADY_CONSTRUCTED,
        "a content type has already been bound for this surface");
    return;
  }

  wl_resource* content_type_resource =
      wl_resource_create(client, &wp_content_type_v1_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(content_type_resource, &content_type_interface,
                    std::make_unique<SurfaceContentType>(surface));
}

const struct wp_content_type_manager_v1_interface
    content_type_manager_interface {
  .destroy = &content_type_manager_destroy,
  .get_surface_content_type = &content_type_manger_get_surface_content_type,
};

}  // namespace

void bind_content_type(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wp_content_type_manager_v1_interface, 1, id);

  wl_resource_set_implementation(resource, &content_type_manager_interface,
                                 data, nullptr);
}

}  // namespace wayland
}  // namespace exo
