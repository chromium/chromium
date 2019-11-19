// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wl_compositor.h"

#include <wayland-server-protocol-core.h>

#include <memory>

#include "base/bind.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"
#include "third_party/skia/include/core/SkRegion.h"

#if defined(OS_CHROMEOS)
#include "components/exo/wayland/zwp_linux_explicit_synchronization.h"
#endif

namespace exo {
namespace wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// wl_region_interface:

void region_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void region_add(wl_client* client,
                wl_resource* resource,
                int32_t x,
                int32_t y,
                int32_t width,
                int32_t height) {
  GetUserDataAs<SkRegion>(resource)->op(SkIRect::MakeXYWH(x, y, width, height),
                                        SkRegion::kUnion_Op);
}

static void region_subtract(wl_client* client,
                            wl_resource* resource,
                            int32_t x,
                            int32_t y,
                            int32_t width,
                            int32_t height) {
  GetUserDataAs<SkRegion>(resource)->op(SkIRect::MakeXYWH(x, y, width, height),
                                        SkRegion::kDifference_Op);
}

const struct wl_region_interface region_implementation = {
    region_destroy, region_add, region_subtract};

////////////////////////////////////////////////////////////////////////////////
// wl_surface_interface:

void surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void surface_attach(wl_client* client,
                    wl_resource* resource,
                    wl_resource* buffer,
                    int32_t x,
                    int32_t y) {
  GetUserDataAs<Surface>(resource)->Attach(
      buffer ? GetUserDataAs<Buffer>(buffer) : nullptr, gfx::Vector2d(x, y));
}

void surface_damage(wl_client* client,
                    wl_resource* resource,
                    int32_t x,
                    int32_t y,
                    int32_t width,
                    int32_t height) {
  GetUserDataAs<Surface>(resource)->Damage(gfx::Rect(x, y, width, height));
}

void HandleSurfaceFrameCallback(wl_resource* resource,
                                base::TimeTicks frame_time) {
  if (!frame_time.is_null()) {
    wl_callback_send_done(resource, TimeTicksToMilliseconds(frame_time));
    // TODO(reveman): Remove this potentially blocking flush and instead watch
    // the file descriptor to be ready for write without blocking.
    wl_client_flush(wl_resource_get_client(resource));
  }
  wl_resource_destroy(resource);
}

void surface_frame(wl_client* client,
                   wl_resource* resource,
                   uint32_t callback) {
  wl_resource* callback_resource =
      wl_resource_create(client, &wl_callback_interface, 1, callback);

  // base::Unretained is safe as the resource owns the callback.
  auto cancelable_callback =
      std::make_unique<base::CancelableCallback<void(base::TimeTicks)>>(
          base::Bind(&HandleSurfaceFrameCallback,
                     base::Unretained(callback_resource)));

  GetUserDataAs<Surface>(resource)->RequestFrameCallback(
      cancelable_callback->callback());

  SetImplementation(callback_resource, nullptr, std::move(cancelable_callback));
}

void surface_set_opaque_region(wl_client* client,
                               wl_resource* resource,
                               wl_resource* region_resource) {
  SkRegion region = region_resource ? *GetUserDataAs<SkRegion>(region_resource)
                                    : SkRegion(SkIRect::MakeEmpty());
  GetUserDataAs<Surface>(resource)->SetOpaqueRegion(cc::Region(region));
}

void surface_set_input_region(wl_client* client,
                              wl_resource* resource,
                              wl_resource* region_resource) {
  Surface* surface = GetUserDataAs<Surface>(resource);
  if (region_resource) {
    surface->SetInputRegion(
        cc::Region(*GetUserDataAs<SkRegion>(region_resource)));
  } else
    surface->ResetInputRegion();
}

void surface_commit(wl_client* client, wl_resource* resource) {
  Surface* surface = GetUserDataAs<Surface>(resource);

#if defined(OS_CHROMEOS)
  if (!linux_surface_synchronization_validate_commit(surface))
    return;
#endif

  surface->Commit();
}

void surface_set_buffer_transform(wl_client* client,
                                  wl_resource* resource,
                                  int32_t transform) {
  Transform buffer_transform;
  switch (transform) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
      buffer_transform = Transform::NORMAL;
      break;
    case WL_OUTPUT_TRANSFORM_90:
      buffer_transform = Transform::ROTATE_90;
      break;
    case WL_OUTPUT_TRANSFORM_180:
      buffer_transform = Transform::ROTATE_180;
      break;
    case WL_OUTPUT_TRANSFORM_270:
      buffer_transform = Transform::ROTATE_270;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      NOTIMPLEMENTED();
      return;
    default:
      wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_TRANSFORM,
                             "buffer transform must be one of the values from "
                             "the wl_output.transform enum ('%d' specified)",
                             transform);
      return;
  }

  GetUserDataAs<Surface>(resource)->SetBufferTransform(buffer_transform);
}

void surface_set_buffer_scale(wl_client* client,
                              wl_resource* resource,
                              int32_t scale) {
  if (scale < 1) {
    wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_SCALE,
                           "buffer scale must be at least one "
                           "('%d' specified)",
                           scale);
    return;
  }

  GetUserDataAs<Surface>(resource)->SetBufferScale(scale);
}

const struct wl_surface_interface surface_implementation = {
    surface_destroy,
    surface_attach,
    surface_damage,
    surface_frame,
    surface_set_opaque_region,
    surface_set_input_region,
    surface_commit,
    surface_set_buffer_transform,
    surface_set_buffer_scale};

////////////////////////////////////////////////////////////////////////////////
// wl_compositor_interface:

void compositor_create_surface(wl_client* client,
                               wl_resource* resource,
                               uint32_t id) {
  std::unique_ptr<Surface> surface =
      GetUserDataAs<Display>(resource)->CreateSurface();

  wl_resource* surface_resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);

  // Set the surface resource property for type-checking downcast support.
  SetSurfaceResource(surface.get(), surface_resource);

  SetImplementation(surface_resource, &surface_implementation,
                    std::move(surface));
}

void compositor_create_region(wl_client* client,
                              wl_resource* resource,
                              uint32_t id) {
  wl_resource* region_resource =
      wl_resource_create(client, &wl_region_interface, 1, id);

  SetImplementation(region_resource, &region_implementation,
                    std::make_unique<SkRegion>());
}

const struct wl_compositor_interface compositor_implementation = {
    compositor_create_surface, compositor_create_region};

}  // namespace

void bind_compositor(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &wl_compositor_interface,
                         std::min(version, kWlCompositorVersion), id);

  wl_resource_set_implementation(resource, &compositor_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
