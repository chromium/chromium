// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_linux_dmabuf.h"
#include "base/memory/raw_ptr.h"

#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-server-protocol.h>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/wayland_dmabuf_feedback_manager.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/linux/drm_util_linux.h"

namespace exo {
namespace wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// wl_buffer_interface:

void buffer_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct wl_buffer_interface buffer_implementation = {buffer_destroy};

void HandleBufferReleaseCallback(wl_resource* resource) {
  wl_buffer_send_release(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

////////////////////////////////////////////////////////////////////////////////
// linux_buffer_params_interface:

struct LinuxBufferParams {
  struct Plane {
    base::ScopedFD fd;
    uint32_t stride;
    uint32_t offset;
    uint64_t modifier;
  };

  explicit LinuxBufferParams(WaylandDmabufFeedbackManager* feedback_manager)
      : feedback_manager(feedback_manager) {}

  const raw_ptr<WaylandDmabufFeedbackManager> feedback_manager;
  std::map<uint32_t, Plane> planes;
};

void linux_buffer_params_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linux_buffer_params_add(wl_client* client,
                             wl_resource* resource,
                             int32_t fd,
                             uint32_t plane_idx,
                             uint32_t offset,
                             uint32_t stride,
                             uint32_t modifier_hi,
                             uint32_t modifier_lo) {
  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  const uint64_t modifier = (static_cast<uint64_t>(modifier_hi) << 32) | modifier_lo;
  LinuxBufferParams::Plane plane{base::ScopedFD(fd), stride, offset, modifier};

  bool inserted = linux_buffer_params->planes
                      .insert(std::make_pair(plane_idx, std::move(plane)))
                      .second;
  if (!inserted) {  // The plane was already there.
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                           "plane already set");
  }
}

bool ValidateLinuxBufferParams(wl_resource* resource,
                               int32_t width,
                               int32_t height,
                               gfx::BufferFormat format,
                               uint32_t flags) {
  if (width <= 0 || height <= 0) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                           "invalid width or height");
    return false;
  }

  if (flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_INTERLACED) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                           "flags not supported");
    return false;
  }

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  size_t num_planes = linux_buffer_params->planes.size();
  if (num_planes == 0) {
    wl_resource_post_error(resource,
                          ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                          "no planes given");
    return false;
  }

  // Validate that we have planes 0..num_planes-1
  for (uint32_t i = 0; i < num_planes; ++i) {
    if (!base::Contains(linux_buffer_params->planes, i)) {
      wl_resource_post_error(resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                             "missing a plane");
      return false;
    }
  }

  // All planes must have the same modifier.
  uint64_t modifier = linux_buffer_params->planes[0].modifier;
  for (uint32_t i = 1; i < num_planes; ++i) {
    if (linux_buffer_params->planes[i].modifier != modifier) {
      wl_resource_post_error(resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                             "all planes must have same modifier");
      return false;
    }
  }

  return true;
}

wl_resource* create_buffer(wl_client* client,
                           wl_resource* resource,
                           uint32_t buffer_id,
                           int32_t width,
                           int32_t height,
                           uint32_t format,
                           uint32_t flags) {
  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  if (!linux_buffer_params->feedback_manager->IsFormatSupported(format)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "format not supported");
    return nullptr;
  }

  gfx::BufferFormat buffer_format = ui::GetBufferFormatFromFourCCFormat(format);
  if (!ValidateLinuxBufferParams(resource, width, height, buffer_format,
                                 flags)) {
    return nullptr;
  }

  gfx::NativePixmapHandle handle;

  handle.modifier = linux_buffer_params->planes[0].modifier;

  for (uint32_t i = 0; i < linux_buffer_params->planes.size(); ++i) {
    auto& plane = linux_buffer_params->planes[i];
    handle.planes.emplace_back(plane.stride, plane.offset, 0,
                               std::move(plane.fd));
  }

  bool y_invert = (flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT) != 0;

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->feedback_manager->GetDisplay()
          ->CreateLinuxDMABufBuffer(gfx::Size(width, height), buffer_format,
                                    std::move(handle), y_invert);
  if (!buffer) {
    zwp_linux_buffer_params_v1_send_failed(resource);
    return nullptr;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, buffer_id);

  buffer->set_release_callback(base::BindRepeating(
      &HandleBufferReleaseCallback, base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));

  return buffer_resource;
}

void linux_buffer_params_create(wl_client* client,
                                wl_resource* resource,
                                int32_t width,
                                int32_t height,
                                uint32_t format,
                                uint32_t flags) {
  wl_resource* buffer_resource =
      create_buffer(client, resource, 0, width, height, format, flags);

  if (buffer_resource)
    zwp_linux_buffer_params_v1_send_created(resource, buffer_resource);
}

void linux_buffer_params_create_immed(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t buffer_id,
                                      int32_t width,
                                      int32_t height,
                                      uint32_t format,
                                      uint32_t flags) {
  create_buffer(client, resource, buffer_id, width, height, format, flags);
}

const struct zwp_linux_buffer_params_v1_interface
    linux_buffer_params_implementation = {
        linux_buffer_params_destroy, linux_buffer_params_add,
        linux_buffer_params_create, linux_buffer_params_create_immed};

////////////////////////////////////////////////////////////////////////////////
// linux_dmabuf_interface:

void linux_dmabuf_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linux_dmabuf_create_params(wl_client* client,
                                wl_resource* resource,
                                uint32_t id) {
  WaylandDmabufFeedbackManager* feedback_manager =
      GetUserDataAs<WaylandDmabufFeedbackManager>(resource);

  std::unique_ptr<LinuxBufferParams> linux_buffer_params =
      std::make_unique<LinuxBufferParams>(feedback_manager);

  wl_resource* linux_buffer_params_resource =
      wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(linux_buffer_params_resource,
                    &linux_buffer_params_implementation,
                    std::move(linux_buffer_params));
}

void linux_dmabuf_get_default_feedback(wl_client* client,
                                       wl_resource* dma_buf_resource,
                                       uint32_t feedback_id) {
  WaylandDmabufFeedbackManager* feedback_manager =
      static_cast<WaylandDmabufFeedbackManager*>(
          wl_resource_get_user_data(dma_buf_resource));

  feedback_manager->GetDefaultFeedback(client, dma_buf_resource, feedback_id);
}

void linux_dmabuf_get_surface_feedback(wl_client* client,
                                       wl_resource* dma_buf_resource,
                                       uint32_t feedback_id,
                                       wl_resource* surface_resource) {
  WaylandDmabufFeedbackManager* feedback_manager =
      static_cast<WaylandDmabufFeedbackManager*>(
          wl_resource_get_user_data(dma_buf_resource));

  feedback_manager->GetSurfaceFeedback(client, dma_buf_resource, feedback_id,
                                       surface_resource);
}

const struct zwp_linux_dmabuf_v1_interface linux_dmabuf_implementation = {
    linux_dmabuf_destroy,
    linux_dmabuf_create_params,
    linux_dmabuf_get_default_feedback,
    linux_dmabuf_get_surface_feedback,
};

}  // namespace

void bind_linux_dmabuf(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  WaylandDmabufFeedbackManager* feedback_manager =
      static_cast<WaylandDmabufFeedbackManager*>(data);

  wl_resource* resource = wl_resource_create(
      client, &zwp_linux_dmabuf_v1_interface,
      std::min(version, feedback_manager->GetVersionSupportedByPlatform()), id);

  wl_resource_set_implementation(resource, &linux_dmabuf_implementation,
                                 feedback_manager, nullptr);

  if (wl_resource_get_version(resource) <
      ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION) {
    feedback_manager->SendFormatsAndModifiers(resource);
  }
}

}  // namespace wayland
}  // namespace exo
