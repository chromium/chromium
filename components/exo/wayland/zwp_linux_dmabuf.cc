// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zwp_linux_dmabuf.h"

#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-server-protocol.h>

#include "base/bind.h"
#include "components/exo/buffer.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server_util.h"
#include "ui/gfx/buffer_format_util.h"

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

const struct dmabuf_supported_format {
  uint32_t dmabuf_format;
  gfx::BufferFormat buffer_format;
} dmabuf_supported_formats[] = {
    {DRM_FORMAT_RGB565, gfx::BufferFormat::BGR_565},
    {DRM_FORMAT_XBGR8888, gfx::BufferFormat::RGBX_8888},
    {DRM_FORMAT_ABGR8888, gfx::BufferFormat::RGBA_8888},
    {DRM_FORMAT_XRGB8888, gfx::BufferFormat::BGRX_8888},
    {DRM_FORMAT_ARGB8888, gfx::BufferFormat::BGRA_8888},
    {DRM_FORMAT_NV12, gfx::BufferFormat::YUV_420_BIPLANAR},
    {DRM_FORMAT_YVU420, gfx::BufferFormat::YVU_420}};

struct LinuxBufferParams {
  struct Plane {
    base::ScopedFD fd;
    uint32_t stride;
    uint32_t offset;
  };

  explicit LinuxBufferParams(Display* display) : display(display) {}

  Display* const display;
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

  LinuxBufferParams::Plane plane{base::ScopedFD(fd), stride, offset};

  const auto& inserted = linux_buffer_params->planes.insert(
      std::pair<uint32_t, LinuxBufferParams::Plane>(plane_idx,
                                                    std::move(plane)));
  if (!inserted.second) {  // The plane was already there.
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
  size_t num_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);

  for (uint32_t i = 0; i < num_planes; ++i) {
    auto plane_it = linux_buffer_params->planes.find(i);
    if (plane_it == linux_buffer_params->planes.end()) {
      wl_resource_post_error(resource,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                             "missing a plane");
      return false;
    }
  }

  if (linux_buffer_params->planes.size() != num_planes) {
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                           "plane idx out of bounds");
    return false;
  }

  return true;
}

void linux_buffer_params_create(wl_client* client,
                                wl_resource* resource,
                                int32_t width,
                                int32_t height,
                                uint32_t format,
                                uint32_t flags) {
  const auto* supported_format = std::find_if(
      std::begin(dmabuf_supported_formats), std::end(dmabuf_supported_formats),
      [format](const dmabuf_supported_format& supported_format) {
        return supported_format.dmabuf_format == format;
      });
  if (supported_format == std::end(dmabuf_supported_formats)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "format not supported");
    return;
  }

  if (!ValidateLinuxBufferParams(resource, width, height,
                                 supported_format->buffer_format, flags))
    return;

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  size_t num_planes =
      gfx::NumberOfPlanesForLinearBufferFormat(supported_format->buffer_format);

  gfx::NativePixmapHandle handle;

  for (uint32_t i = 0; i < num_planes; ++i) {
    auto plane_it = linux_buffer_params->planes.find(i);
    LinuxBufferParams::Plane& plane = plane_it->second;
    handle.planes.emplace_back(plane.stride, plane.offset, 0,
                               std::move(plane.fd));
  }

  bool y_invert = (flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT) != 0;

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->display->CreateLinuxDMABufBuffer(
          gfx::Size(width, height), supported_format->buffer_format,
          std::move(handle), y_invert);
  if (!buffer) {
    zwp_linux_buffer_params_v1_send_failed(resource);
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, 0);

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));

  zwp_linux_buffer_params_v1_send_created(resource, buffer_resource);
}

void linux_buffer_params_create_immed(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t buffer_id,
                                      int32_t width,
                                      int32_t height,
                                      uint32_t format,
                                      uint32_t flags) {
  const auto* supported_format = std::find_if(
      std::begin(dmabuf_supported_formats), std::end(dmabuf_supported_formats),
      [format](const dmabuf_supported_format& supported_format) {
        return supported_format.dmabuf_format == format;
      });
  if (supported_format == std::end(dmabuf_supported_formats)) {
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "format not supported");
    return;
  }

  if (!ValidateLinuxBufferParams(resource, width, height,
                                 supported_format->buffer_format, flags))
    return;

  LinuxBufferParams* linux_buffer_params =
      GetUserDataAs<LinuxBufferParams>(resource);

  size_t num_planes =
      gfx::NumberOfPlanesForLinearBufferFormat(supported_format->buffer_format);

  gfx::NativePixmapHandle handle;

  for (uint32_t i = 0; i < num_planes; ++i) {
    auto plane_it = linux_buffer_params->planes.find(i);
    LinuxBufferParams::Plane& plane = plane_it->second;
    handle.planes.emplace_back(plane.stride, plane.offset, 0,
                               std::move(plane.fd));
  }

  bool y_invert = flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT;

  std::unique_ptr<Buffer> buffer =
      linux_buffer_params->display->CreateLinuxDMABufBuffer(
          gfx::Size(width, height), supported_format->buffer_format,
          std::move(handle), y_invert);
  if (!buffer) {
    // On import failure in case of a create_immed request, the protocol
    // allows us to raise a fatal error from zwp_linux_dmabuf_v1 version 2+.
    wl_resource_post_error(resource,
                           ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
                           "dmabuf import failed");
    return;
  }

  wl_resource* buffer_resource =
      wl_resource_create(client, &wl_buffer_interface, 1, buffer_id);

  buffer->set_release_callback(base::Bind(&HandleBufferReleaseCallback,
                                          base::Unretained(buffer_resource)));

  SetImplementation(buffer_resource, &buffer_implementation, std::move(buffer));
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
  std::unique_ptr<LinuxBufferParams> linux_buffer_params =
      std::make_unique<LinuxBufferParams>(GetUserDataAs<Display>(resource));

  wl_resource* linux_buffer_params_resource =
      wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
                         wl_resource_get_version(resource), id);

  SetImplementation(linux_buffer_params_resource,
                    &linux_buffer_params_implementation,
                    std::move(linux_buffer_params));
}

const struct zwp_linux_dmabuf_v1_interface linux_dmabuf_implementation = {
    linux_dmabuf_destroy, linux_dmabuf_create_params};

}  // namespace

void bind_linux_dmabuf(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zwp_linux_dmabuf_v1_interface,
                         std::min(version, kZwpLinuxDmabufVersion), id);

  wl_resource_set_implementation(resource, &linux_dmabuf_implementation, data,
                                 nullptr);

  for (const auto& supported_format : dmabuf_supported_formats)
    zwp_linux_dmabuf_v1_send_format(resource, supported_format.dmabuf_format);
}

}  // namespace wayland
}  // namespace exo
