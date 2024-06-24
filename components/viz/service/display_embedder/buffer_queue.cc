// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/buffer_queue.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"

namespace viz {

BufferQueue::BufferQueue(SkiaOutputSurface* skia_output_surface,
                         gpu::SurfaceHandle surface_handle,
                         size_t number_of_buffers,
                         bool is_protected)
    : skia_output_surface_(skia_output_surface),
      surface_handle_(surface_handle),
      number_of_buffers_(number_of_buffers),
      is_protected_(is_protected) {}

BufferQueue::~BufferQueue() {
  FreeAllBuffers();
}

gpu::Mailbox BufferQueue::GetCurrentBuffer() {
  if (!current_buffer_) {
    current_buffer_ = GetNextBuffer();
  }
  DCHECK(current_buffer_);
  return current_buffer_->mailbox;
}

void BufferQueue::UpdateBufferDamage(const gfx::Rect& damage) {
  if (displayed_buffer_) {
    displayed_buffer_->damage.Union(damage);
  }
  for (auto& buffer : available_buffers_) {
    buffer->damage.Union(damage);
  }
  for (auto& buffer : in_flight_buffers_) {
    if (buffer) {
      buffer->damage.Union(damage);
    }
  }
}

gfx::Rect BufferQueue::CurrentBufferDamage() const {
  if (current_buffer_) {
    return current_buffer_->damage;
  }

  // In case there is no current_buffer_, we get the damage from the buffer
  // that will be set as current_buffer_ by the next call to GetNextBuffer.
  if (!available_buffers_.empty()) {
    return available_buffers_.front()->damage;
  }

  // If we can't determine which buffer will be the next current_buffer_, we
  // conservatively invalidate the whole buffer.
  return gfx::Rect(size_);
}

void BufferQueue::SwapBuffers(const gfx::Rect& damage) {
  UpdateBufferDamage(damage);
  if (current_buffer_) {
    current_buffer_->damage = gfx::Rect();
  }
  // Note: In the case of an empty-swap frame, GetCurrentBuffer() was not called
  // this frame and current_buffer_ will be nullptr. We will still push nullptr
  // into in_flight_buffers_ so the queue is kept in sync when
  // SwapBuffersComplete() is called.
  in_flight_buffers_.push_back(std::move(current_buffer_));
}

void BufferQueue::SwapBuffersComplete() {
  DCHECK(!in_flight_buffers_.empty());

  if (displayed_buffer_) {
    available_buffers_.push_back(std::move(displayed_buffer_));
  }
  displayed_buffer_ = std::move(in_flight_buffers_.front());
  in_flight_buffers_.pop_front();

  if (buffers_can_be_purged_) {
    for (auto& buffer : available_buffers_) {
      if (SetBufferPurgeable(*buffer, true)) {
        // Set a single available buffer to purgeable each swap.
        break;
      }
    }
  }
}

void BufferQueue::SwapBuffersSkipped(const gfx::Rect& damage) {
  UpdateBufferDamage(damage);
}

bool BufferQueue::Reshape(const gfx::Size& size,
                          const gfx::ColorSpace& color_space,
                          SharedImageFormat format) {
  if (size == size_ && color_space == color_space_ && format == format_) {
    return false;
  }

  size_ = size;
  color_space_ = color_space;
  format_ = format;

  if (buffers_destroyed_) {
    return true;
  }

  if (buffers_can_be_purged_) {
    // If buffers are purgeable wait to recreate until they will be used again.
    DestroyBuffers();
    return true;
  }

  FreeAllBuffers();
  AllocateBuffers(number_of_buffers_);

  return true;
}

void BufferQueue::RecreateBuffers() {
  if (buffers_destroyed_) {
    return;
  }

  if (buffers_can_be_purged_) {
    // If buffers are purgeable wait to recreate until they will be used again.
    DestroyBuffers();
    return;
  }

  FreeAllBuffers();
  AllocateBuffers(number_of_buffers_);
}

void BufferQueue::FreeAllBuffers() {
  FreeBuffer(std::move(displayed_buffer_));
  FreeBuffer(std::move(current_buffer_));

  // This is intentionally not emptied since the swap buffers acks are still
  // expected to arrive.
  for (auto& buffer : in_flight_buffers_) {
    FreeBuffer(std::move(buffer));
  }

  for (auto& buffer : available_buffers_) {
    FreeBuffer(std::move(buffer));
  }
  available_buffers_.clear();
}

void BufferQueue::FreeBuffer(std::unique_ptr<AllocatedBuffer> buffer) {
  if (!buffer) {
    return;
  }
  DCHECK(!buffer->mailbox.IsZero());
  skia_output_surface_->DestroySharedImage(buffer->mailbox);
}

bool BufferQueue::SetBufferPurgeable(AllocatedBuffer& buffer, bool purgeable) {
  if (buffer.purgeable == purgeable) {
    return false;
  }

  skia_output_surface_->SetSharedImagePurgeable(buffer.mailbox, purgeable);
  buffer.purgeable = true;
  return true;
}

void BufferQueue::AllocateBuffers(size_t n) {
  DCHECK(format_);

  const gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      (is_protected_ ? gpu::SHARED_IMAGE_USAGE_PROTECTED_VIDEO
                     : gpu::SharedImageUsageSet());

  available_buffers_.reserve(available_buffers_.size() + n);
  for (size_t i = 0; i < n; ++i) {
    const gpu::Mailbox mailbox = skia_output_surface_->CreateSharedImage(
        format_.value(), size_, color_space_, RenderPassAlphaType::kPremul,
        usage, "VizBufferQueue", surface_handle_);
    DCHECK(!mailbox.IsZero());

    available_buffers_.push_back(
        std::make_unique<AllocatedBuffer>(mailbox, gfx::Rect(size_)));
  }
}

std::unique_ptr<BufferQueue::AllocatedBuffer> BufferQueue::GetNextBuffer() {
  RecreateBuffersIfDestroyed();

  DCHECK(!available_buffers_.empty());

  std::unique_ptr<AllocatedBuffer> buffer =
      std::move(available_buffers_.front());
  available_buffers_.pop_front();
  return buffer;
}

gpu::Mailbox BufferQueue::GetLastSwappedBuffer() {
  if (buffers_destroyed_) {
    // Buffers will not be destroyed on platforms where we need to use a buffer
    // for overlay testing (Ash).
    return gpu::Mailbox();
  }

  // The last swapped buffer will generally be in `displayed_buffer_`, unless
  // the last completed swap was empty or there haven't been any completed swaps
  // since Reshape() was last called.
  if (displayed_buffer_) {
    return displayed_buffer_->mailbox;
  }

  // If displayed_buffer_ is null then any available buffer will do.
  if (!available_buffers_.empty()) {
    return available_buffers_.back()->mailbox;
  }

  // If there's nothing displayed or available, then we should have no buffers
  // allocated because Reshape() hasn't been called yet, so a zero-mailbox is
  // returned.
  // If any buffers are in flight at this point then BufferQueue is being used
  // incorrectly. We should not be Swap()ing all available buffers before
  // receiving any SwapBuffersComplete() calls.
  DCHECK(in_flight_buffers_.empty());
  return gpu::Mailbox();
}

void BufferQueue::EnsureMinNumberOfBuffers(size_t n) {
  if (n <= number_of_buffers_) {
    return;
  }

  // If Reshape hasn't been called yet we can't allocate the buffers.
  if (!size_.IsEmpty() && !buffers_destroyed_) {
    AllocateBuffers(n - number_of_buffers_);
  }
  number_of_buffers_ = n;
}

void BufferQueue::DestroyBuffers() {
  if (buffers_destroyed_) {
    return;
  }
  buffers_destroyed_ = true;
  destroyed_timer_ = base::ElapsedTimer();
  FreeAllBuffers();
}

void BufferQueue::SetBuffersPurgeable() {
  if (buffers_can_be_purged_) {
    return;
  }
  buffers_can_be_purged_ = true;
}

void BufferQueue::RecreateBuffersIfDestroyed() {
  if (buffers_can_be_purged_) {
    // Mark buffers as not purgeable. It's possible they were destroyed and
    // `available_buffers_` is empty.
    buffers_can_be_purged_ = false;
    for (auto& buffer : available_buffers_) {
      SetBufferPurgeable(*buffer, false);
    }
  }

  if (buffers_destroyed_) {
    buffers_destroyed_ = false;
    AllocateBuffers(number_of_buffers_);
    base::TimeDelta elapsed = destroyed_timer_->Elapsed();
    UMA_HISTOGRAM_TIMES("Compositing.BufferQueue.TimeUntilBuffersRecreatedMs",
                        elapsed);
    destroyed_timer_.reset();
  }
}

BufferQueue::AllocatedBuffer::AllocatedBuffer(const gpu::Mailbox& mailbox,
                                              const gfx::Rect& rect)
    : mailbox(mailbox), damage(rect) {}

BufferQueue::AllocatedBuffer::~AllocatedBuffer() = default;

}  // namespace viz
