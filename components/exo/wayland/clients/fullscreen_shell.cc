// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/fullscreen_shell.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gl/gl_bindings.h"

namespace exo {
namespace wayland {
namespace clients {

namespace {

void FrameCallback(void* data, wl_callback* callback, uint32_t time) {
  bool* frame_callback_pending = static_cast<bool*>(data);
  *frame_callback_pending = false;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FullscreenClient, public:

FullscreenClient::FullscreenClient() = default;

FullscreenClient::~FullscreenClient() = default;

bool FullscreenClient::Run(const InitParams& params) {
  wl_callback_listener frame_listener = {FrameCallback};

  // Wait for screen mode to be received
  while (wl_display_dispatch(display_.get()) != -1) {
    if (done_receiving_modes_ && !has_mode_) {
      LOG(ERROR) << "Did not receive screen mode";
      return false;
    } else if (has_mode_) {
      break;
    }
  }

  AllocateBuffers(params);

  do {
    if (frame_callback_pending_)
      continue;

    if (frame_count_ == frames_)
      break;

    Paint(frame_listener);

  } while (wl_display_dispatch(display_.get()) != -1);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FullscreenClient: Private

void FullscreenClient::AllocateBuffers(const InitParams& params) {
  for (size_t i = 0; i < params.num_buffers; ++i) {
    auto buffer =
        CreateBuffer(size_, params.drm_format, params.bo_usage,
                     /*add_buffer_listener=*/!params.use_release_fences);
    if (!buffer) {
      LOG(ERROR) << "Failed to create buffer";
      return;
    }
    buffers_.push_back(std::move(buffer));
  }
}

void FullscreenClient::Paint(const wl_callback_listener& frame_listener) {
  Buffer* buffer = DequeueBuffer();
  if (!buffer)
    return;

  ++frame_count_;

  int left = point_.x();
  int top = point_.y();
  int right = point_.x() + square_size_.width();
  int bottom = point_.y() + square_size_.height();

  if (right >= size_.width() || left <= 0) {
    dir_x_ *= -1;
  }

  if (top <= 0 || bottom >= size_.height()) {
    dir_y_ *= -1;
  }

  point_.set_x(point_.x() + dir_x_ * step_size_);
  point_.set_y(point_.y() + dir_y_ * step_size_);

  SkCanvas* canvas = buffer->sk_surface->getCanvas();
  canvas->clear(color_);
  const SkRect rect = gfx::RectToSkRect(gfx::Rect{point_, square_size_});
  const SkPaint paint;
  canvas->drawRect(rect, paint);

  if (gr_context_) {
    gr_context_->flushAndSubmit();
    glFinish();
  }

  wl_surface_set_buffer_scale(surface_.get(), scale_);
  wl_surface_set_buffer_transform(surface_.get(), transform_);
  wl_surface_damage(surface_.get(), 0, 0, surface_size_.width(),
                    surface_size_.height());
  wl_surface_attach(surface_.get(), buffer->buffer.get(), 0, 0);

  // Set up the frame callback.
  frame_callback_pending_ = true;
  frame_callback_.reset(wl_surface_frame(surface_.get()));
  wl_callback_add_listener(frame_callback_.get(), &frame_listener,
                           &frame_callback_pending_);

  wl_surface_commit(surface_.get());
  wl_display_flush(display_.get());
}

void FullscreenClient::HandleDown(void* data,
                                  struct wl_touch* wl_touch,
                                  uint32_t serial,
                                  uint32_t time,
                                  struct wl_surface* surface,
                                  int32_t id,
                                  wl_fixed_t x,
                                  wl_fixed_t y) {
  if (color_ == SK_ColorBLUE) {
    color_ = SK_ColorRED;
  } else {
    color_ = SK_ColorBLUE;
  }
}

void FullscreenClient::HandleMode(void* data,
                                  struct wl_output* wl_output,
                                  uint32_t flags,
                                  int32_t width,
                                  int32_t height,
                                  int32_t refresh) {
  if ((WL_OUTPUT_MODE_CURRENT & flags) != WL_OUTPUT_MODE_CURRENT)
    return;

  size_.SetSize(width, height);
  switch (transform_) {
    case WL_OUTPUT_TRANSFORM_NORMAL:
    case WL_OUTPUT_TRANSFORM_180:
      surface_size_.SetSize(width, height);
      break;
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
      surface_size_.SetSize(height, width);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  std::unique_ptr<wl_region> opaque_region(static_cast<wl_region*>(
      wl_compositor_create_region(globals_.compositor.get())));

  if (!opaque_region) {
    LOG(ERROR) << "Can't create region";
    return;
  }

  wl_region_add(opaque_region.get(), 0, 0, surface_size_.width(),
                surface_size_.height());
  wl_surface_set_opaque_region(surface_.get(), opaque_region.get());
  wl_surface_set_input_region(surface_.get(), opaque_region.get());

  has_mode_ = true;
}

void FullscreenClient::HandleDone(void* data, struct wl_output* wl_output) {
  done_receiving_modes_ = true;
}

}  // namespace clients
}  // namespace wayland
}  // namespace exo
