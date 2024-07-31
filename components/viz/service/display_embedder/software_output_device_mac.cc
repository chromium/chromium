// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/display_embedder/software_output_device_mac.h"

#include <memory>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/mac/io_surface.h"

namespace viz {

SoftwareOutputDeviceMac::Buffer::Buffer() = default;
SoftwareOutputDeviceMac::Buffer::~Buffer() = default;

SoftwareOutputDeviceMac::SoftwareOutputDeviceMac(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : SoftwareOutputDevice(std::move(task_runner)) {}

SoftwareOutputDeviceMac::~SoftwareOutputDeviceMac() {}

void SoftwareOutputDeviceMac::Resize(const gfx::Size& pixel_size,
                                     float scale_factor) {
  if (pixel_size_ == pixel_size && scale_factor_ == scale_factor)
    return;

  pixel_size_ = pixel_size;
  scale_factor_ = scale_factor;

  DiscardBackbuffer();
}

void SoftwareOutputDeviceMac::UpdateAndCopyBufferDamage(
    Buffer* previous_paint_buffer,
    const SkRegion& new_damage_region) {
  TRACE_EVENT0("browser", "CopyPreviousBufferDamage");

  // Expand the |accumulated_damage| of all buffers to include this frame's
  // damage.
  for (auto& buffer : buffer_queue_)
    buffer->accumulated_damage.op(new_damage_region, SkRegion::kUnion_Op);

  // Compute the region to copy from |previous_paint_buffer| to
  // |current_paint_buffer_| by subtracting |new_damage_region| (which we will
  // be painting) from |current_paint_buffer_|'s |accumulated_damage|.
  SkRegion copy_region;
  current_paint_buffer_->accumulated_damage.swap(copy_region);
  bool copy_region_nonempty =
      copy_region.op(new_damage_region, SkRegion::kDifference_Op);
  last_copy_region_for_testing_ = copy_region;
  if (!copy_region_nonempty)
    return;

  // If we have anything to copy, we had better have a buffer to copy it from.
  if (!previous_paint_buffer) {
    DLOG(ERROR) << "No previous paint buffer to copy accumulated damage from.";
    last_copy_region_for_testing_.setEmpty();
    return;
  }

  // It is possible for |previous_paint_buffer| to equal
  // |current_paint_buffer_|, but if it does, we should not need to do a copy.
  CHECK_NE(previous_paint_buffer, current_paint_buffer_);

  IOSurfaceRef previous_io_surface = previous_paint_buffer->io_surface.get();

  {
    TRACE_EVENT0("browser", "IOSurfaceLock for software copy");
    IOReturn io_result = IOSurfaceLock(
        previous_io_surface, kIOSurfaceLockReadOnly | kIOSurfaceLockAvoidSync,
        nullptr);
    if (io_result) {
      DLOG(ERROR) << "Failed to lock previous IOSurface " << io_result;
      return;
    }
  }

  uint8_t* pixels =
      static_cast<uint8_t*>(IOSurfaceGetBaseAddress(previous_io_surface));
  size_t stride = IOSurfaceGetBytesPerRow(previous_io_surface);
  size_t bytes_per_element = 4;
  for (SkRegion::Iterator it(copy_region); !it.done(); it.next()) {
    const SkIRect& rect = it.rect();
    current_paint_canvas_->writePixels(
        SkImageInfo::MakeN32Premul(rect.width(), rect.height()),
        pixels + bytes_per_element * rect.x() + stride * rect.y(), stride,
        rect.x(), rect.y());
  }

  {
    TRACE_EVENT0("browser", "IOSurfaceUnlock");
    IOReturn io_result = IOSurfaceUnlock(
        previous_io_surface, kIOSurfaceLockReadOnly | kIOSurfaceLockAvoidSync,
        nullptr);
    if (io_result)
      DLOG(ERROR) << "Failed to unlock previous IOSurface " << io_result;
  }
}

SkCanvas* SoftwareOutputDeviceMac::BeginPaint(
    const gfx::Rect& new_damage_rect) {
  // Record the previous paint buffer.
  Buffer* previous_paint_buffer =
      buffer_queue_.empty() ? nullptr : buffer_queue_.back().get();

  // Find any buffer in the queue that is not in use by the window server, and
  // re-use it as the buffer for this paint. Note that this can be any buffer in
  // any position in the list.
  for (auto iter = buffer_queue_.begin(); iter != buffer_queue_.end(); ++iter) {
    Buffer* iter_buffer = iter->get();
    if (IOSurfaceIsInUse(iter_buffer->io_surface.get())) {
      continue;
    }
    current_paint_buffer_ = iter_buffer;
    buffer_queue_.splice(buffer_queue_.end(), buffer_queue_, iter);
    break;
  }

  // If we failed to find a suitable buffer, allocate a new one, and initialize
  // it with complete damage.
  if (!current_paint_buffer_) {
    std::unique_ptr<Buffer> new_buffer(new Buffer);
    new_buffer->io_surface =
        gfx::CreateIOSurface(pixel_size_, gfx::BufferFormat::BGRA_8888);
    if (!new_buffer->io_surface)
      return nullptr;
    // Set the initial damage to be the full buffer.
    new_buffer->accumulated_damage.setRect(
        gfx::RectToSkIRect(gfx::Rect(pixel_size_)));
    current_paint_buffer_ = new_buffer.get();
    buffer_queue_.push_back(std::move(new_buffer));
  }
  DCHECK(current_paint_buffer_);

  // Animating in steady-state should require no more than 4 buffers. If we have
  // more than that, then purge the older buffers (the window server will
  // continue to hold on to the IOSurfaces as long as is needed, so the
  // consequence will be extra allocate-free cycles).
  size_t kMaxBuffers = 4;
  while (buffer_queue_.size() > kMaxBuffers)
    buffer_queue_.pop_front();

  // Lock the |current_paint_buffer_|'s IOSurface and wrap it in
  // |current_paint_canvas_|.
  {
    TRACE_EVENT0("browser", "IOSurfaceLock for software paint");
    IOReturn io_result = IOSurfaceLock(current_paint_buffer_->io_surface.get(),
                                       kIOSurfaceLockAvoidSync, nullptr);
    if (io_result) {
      DLOG(ERROR) << "Failed to lock IOSurface " << io_result;
      current_paint_buffer_ = nullptr;
      return nullptr;
    }
  }
  {
    SkPMColor* pixels = static_cast<SkPMColor*>(
        IOSurfaceGetBaseAddress(current_paint_buffer_->io_surface.get()));
    size_t stride =
        IOSurfaceGetBytesPerRow(current_paint_buffer_->io_surface.get());
    current_paint_canvas_ = SkCanvas::MakeRasterDirectN32(
        pixel_size_.width(), pixel_size_.height(), pixels, stride);
  }

  UpdateAndCopyBufferDamage(previous_paint_buffer,
                            SkRegion(gfx::RectToSkIRect(new_damage_rect)));

  return current_paint_canvas_.get();
}

void SoftwareOutputDeviceMac::EndPaint() {
  SoftwareOutputDevice::EndPaint();
  if (!current_paint_buffer_)
    return;

  {
    TRACE_EVENT0("browser", "IOSurfaceUnlock");
    IOReturn io_result =
        IOSurfaceUnlock(current_paint_buffer_->io_surface.get(),
                        kIOSurfaceLockAvoidSync, nullptr);
    if (io_result)
      DLOG(ERROR) << "Failed to unlock IOSurface " << io_result;
  }
  current_paint_canvas_.reset();

  if (client_) {
    gfx::CALayerParams ca_layer_params;
    ca_layer_params.is_empty = false;
    ca_layer_params.scale_factor = scale_factor_;
    ca_layer_params.pixel_size = pixel_size_;
    ca_layer_params.io_surface_mach_port.reset(
        IOSurfaceCreateMachPort(current_paint_buffer_->io_surface.get()));
    client_->SoftwareDeviceUpdatedCALayerParams(ca_layer_params);
  }

  current_paint_buffer_ = nullptr;
}

void SoftwareOutputDeviceMac::DiscardBackbuffer() {
  buffer_queue_.clear();
}

void SoftwareOutputDeviceMac::EnsureBackbuffer() {}

gfx::VSyncProvider* SoftwareOutputDeviceMac::GetVSyncProvider() {
  return nullptr;
}

}  // namespace viz
