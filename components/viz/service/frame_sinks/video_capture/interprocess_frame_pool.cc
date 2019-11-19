// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/interprocess_frame_pool.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"

using media::VideoFrame;
using media::VideoPixelFormat;

namespace viz {

// static
constexpr base::TimeDelta InterprocessFramePool::kMinLoggingPeriod;

InterprocessFramePool::InterprocessFramePool(int capacity)
    : capacity_(std::max(capacity, 0)) {
  DCHECK_GT(capacity_, 0u);
}

InterprocessFramePool::~InterprocessFramePool() = default;

scoped_refptr<VideoFrame> InterprocessFramePool::ReserveVideoFrame(
    VideoPixelFormat format,
    const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const size_t bytes_required = VideoFrame::AllocationSize(format, size);

  // Look for an available buffer that's large enough. If one is found, wrap it
  // in a VideoFrame and return it.
  for (auto it = available_buffers_.rbegin(); it != available_buffers_.rend();
       ++it) {
    if (it->mapping.size() < bytes_required) {
      continue;
    }
    PooledBuffer taken = std::move(*it);
    available_buffers_.erase(it.base() - 1);
    return WrapBuffer(std::move(taken), format, size);
  }

  // Look for the largest available buffer, reallocate it, wrap it in a
  // VideoFrame and return it.
  while (!available_buffers_.empty()) {
    const auto it =
        std::max_element(available_buffers_.rbegin(), available_buffers_.rend(),
                         [](const PooledBuffer& a, const PooledBuffer& b) {
                           return a.mapping.size() < b.mapping.size();
                         });
    if (it->mapping.memory() == marked_frame_buffer_)
      marked_frame_buffer_ = nullptr;
    available_buffers_.erase(it.base() - 1);  // Release before allocating more.
    PooledBuffer reallocated =
        mojo::CreateReadOnlySharedMemoryRegion(bytes_required);
    if (!reallocated.IsValid()) {
      LOG_IF(WARNING, CanLogSharedMemoryFailure())
          << "Failed to re-allocate " << bytes_required << " bytes.";
      continue;  // Try again after freeing the next-largest buffer.
    }
    return WrapBuffer(std::move(reallocated), format, size);
  }

  // There are no available buffers. If the pool is at max capacity, punt.
  // Otherwise, allocate a new buffer, wrap it in a VideoFrame and return it.
  if (utilized_buffers_.size() >= capacity_) {
    return nullptr;
  }
  PooledBuffer additional =
      mojo::CreateReadOnlySharedMemoryRegion(bytes_required);
  if (!additional.IsValid()) {
    LOG_IF(WARNING, CanLogSharedMemoryFailure())
        << "Failed to allocate " << bytes_required << " bytes.";
    return nullptr;
  }
  return WrapBuffer(std::move(additional), format, size);
}

void InterprocessFramePool::MarkFrame(const media::VideoFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  marked_frame_buffer_ = frame.data(0);
  marked_frame_size_ = frame.coded_size();
  marked_frame_color_space_ = frame.ColorSpace();
  marked_frame_pixel_format_ = frame.format();
}

void InterprocessFramePool::ClearFrameMarking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  marked_frame_buffer_ = nullptr;
}

bool InterprocessFramePool::HasMarkedFrameWithSize(
    const gfx::Size& size) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return marked_frame_buffer_ != nullptr && marked_frame_size_ == size;
}

scoped_refptr<VideoFrame>
InterprocessFramePool::ResurrectOrDuplicateContentFromMarkedFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!marked_frame_buffer_)
    return nullptr;

  const auto it =
      std::find_if(available_buffers_.rbegin(), available_buffers_.rend(),
                   [this](const PooledBuffer& candidate) {
                     return candidate.mapping.memory() == marked_frame_buffer_;
                   });

  // If the buffer is available, use it directly.
  if (it != available_buffers_.rend()) {
    // Wrap the buffer in a VideoFrame and return it.
    PooledBuffer resurrected = std::move(*it);
    available_buffers_.erase(it.base() - 1);
    auto frame = WrapBuffer(std::move(resurrected), marked_frame_pixel_format_,
                            marked_frame_size_);
    frame->set_color_space(marked_frame_color_space_);
    return frame;
  }

  // Buffer is currently in use. Reserve a new buffer and copy the contents
  // over.
  auto frame =
      ReserveVideoFrame(marked_frame_pixel_format_, marked_frame_size_);
  // The call to ReserverVideoFrame should not have cleared
  // |marked_frame_buffer_|, because that buffer is currently in use.
  DCHECK(marked_frame_buffer_);
  if (!frame)
    return nullptr;
#if DCHECK_IS_ON()
  // Sanity check that |marked_frame_buffer_| indeed corresponds to a buffer in
  // |utilized_buffers_|. If MarkFrame() was erroneously called with a frame
  // that did not belong to this pool or was otherwise tampered with, this might
  // not be the case.
  const auto source_it = std::find_if(
      utilized_buffers_.rbegin(), utilized_buffers_.rend(),
      [this](const std::pair<const media::VideoFrame*,
                             base::ReadOnlySharedMemoryRegion>& candidate) {
        return candidate.first->data(0) == marked_frame_buffer_;
      });
  DCHECK(source_it != utilized_buffers_.rend());
#endif  // DCHECK_IS_ON()

  // Copy the contents over.
  const size_t num_bytes_to_copy = VideoFrame::AllocationSize(
      marked_frame_pixel_format_, marked_frame_size_);
  memcpy(frame->data(0), marked_frame_buffer_, num_bytes_to_copy);

  frame->set_color_space(marked_frame_color_space_);
  return frame;
}

base::ReadOnlySharedMemoryRegion InterprocessFramePool::CloneHandleForDelivery(
    const VideoFrame* frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto it = utilized_buffers_.find(frame);
  DCHECK(it != utilized_buffers_.end());

  return it->second.Duplicate();
}

float InterprocessFramePool::GetUtilization() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return static_cast<float>(utilized_buffers_.size()) / capacity_;
}

scoped_refptr<VideoFrame> InterprocessFramePool::WrapBuffer(
    PooledBuffer pooled_buffer,
    VideoPixelFormat format,
    const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pooled_buffer.IsValid());

  // Create the VideoFrame wrapper. The two components of |pooled_buffer| are
  // split: The shared memory handle is moved off to the side (in a
  // |utilized_buffers_| map entry), while the writable mapping is transferred
  // to the VideoFrame. When the VideoFrame goes out-of-scope, a destruction
  // observer will re-assemble the PooledBuffer from these two components and
  // return it to the |available_buffers_| pool.
  //
  // The VideoFrame could be held, externally, beyond the lifetime of this
  // InterprocessFramePool. However, this is safe because 1) the use of a
  // WeakPtr cancels the callback that would return the buffer back to the pool,
  // and 2) the mapped memory remains valid until the
  // WritableSharedMemoryMapping goes out-of-scope (when the OnceClosure is
  // destroyed).
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalData(
      format, size, gfx::Rect(size), size,
      static_cast<uint8_t*>(pooled_buffer.mapping.memory()),
      pooled_buffer.mapping.size(), base::TimeDelta());
  DCHECK(frame);
  // Sanity-check the assumption being made for SetMarkedBuffer():
  DCHECK_EQ(frame->data(0), pooled_buffer.mapping.memory());
  utilized_buffers_.emplace(frame.get(), std::move(pooled_buffer.region));
  frame->AddDestructionObserver(
      base::BindOnce(&InterprocessFramePool::OnFrameWrapperDestroyed,
                     weak_factory_.GetWeakPtr(), base::Unretained(frame.get()),
                     std::move(pooled_buffer.mapping)));
  return frame;
}

void InterprocessFramePool::OnFrameWrapperDestroyed(
    const VideoFrame* frame,
    base::WritableSharedMemoryMapping mapping) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(mapping.IsValid());

  // Return the buffer to the pool by moving the PooledBuffer back into
  // |available_buffers_|.
  const auto it = utilized_buffers_.find(frame);
  DCHECK(it != utilized_buffers_.end());
  available_buffers_.emplace_back(
      PooledBuffer{std::move(it->second), std::move(mapping)});
  DCHECK(available_buffers_.back().IsValid());
  utilized_buffers_.erase(it);
  DCHECK_LE(available_buffers_.size() + utilized_buffers_.size(), capacity_);
}

bool InterprocessFramePool::CanLogSharedMemoryFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::TimeTicks now = base::TimeTicks::Now();
  if ((now - last_fail_log_time_) >= kMinLoggingPeriod) {
    last_fail_log_time_ = now;
    return true;
  }
  return false;
}

}  // namespace viz
