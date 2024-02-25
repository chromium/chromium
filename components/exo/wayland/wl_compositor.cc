// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wl_compositor.h"

#include <memory>

#include "base/functional/bind.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/zwp_linux_explicit_synchronization.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/display/types/display_constants.h"

namespace exo {
class Server;
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
  auto cancelable_callback = std::make_unique<
      base::CancelableRepeatingCallback<void(base::TimeTicks)>>(
      base::BindRepeating(&HandleSurfaceFrameCallback,
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

  if (!linux_surface_synchronization_validate_commit(surface))
    return;

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
      buffer_transform = Transform::FLIPPED;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
      buffer_transform = Transform::FLIPPED_ROTATE_90;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      buffer_transform = Transform::FLIPPED_ROTATE_180;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      buffer_transform = Transform::FLIPPED_ROTATE_270;
      break;
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

bool HandleSurfaceLeaveEnterCallback(Server* server,
                                     wl_resource* resource,
                                     int64_t old_display_id,
                                     int64_t new_display_id) {
  auto* client = wl_resource_get_client(resource);
  if (old_display_id != display::kInvalidDisplayId) {
    auto* old_output = server->GetOutputResource(client, old_display_id);
    if (old_output) {
      wl_surface_send_leave(resource, old_output);
      wl_client_flush(client);
    }
  }
  if (new_display_id != display::kInvalidDisplayId) {
    auto* new_output = server->GetOutputResource(client, new_display_id);
    if (!new_output)
      return false;
    wl_surface_send_enter(resource, new_output);
    wl_client_flush(client);
  }
  return true;
}

void compositor_create_surface(wl_client* client,
                               wl_resource* resource,
                               uint32_t id) {
  Server* server = GetUserDataAs<Server>(resource);
  Display* display = server->GetDisplay();
  std::unique_ptr<Surface> surface = display->CreateSurface();

  wl_resource* surface_resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);

  surface->set_leave_enter_callback(
      base::RepeatingCallback<bool(int64_t, int64_t)>(base::BindRepeating(
          &HandleSurfaceLeaveEnterCallback, base::Unretained(server),
          base::Unretained(surface_resource))));

  // Set the surface resource property for type-checking downcast support.
  SetSurfaceResource(surface.get(), surface_resource);

  // Notify after the surface is initialized.
  if (display->seat()) {
    display->seat()->NotifySurfaceCreated(surface.get());
  }

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
