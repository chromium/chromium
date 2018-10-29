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
    : capacity_(std::max(capacity, 0)), weak_factory_(this) {
  DCHECK_GT(capacity_, 0u);
}

InterprocessFramePool::~InterprocessFramePool() = default;

scoped_refptr<VideoFrame> InterprocessFramePool::ReserveVideoFrame(
    VideoPixelFormat format,
    const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Calling this method is a signal that there is no intention of resurrecting
  // the last frame.
  resurrectable_buffer_memory_ = nullptr;

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

scoped_refptr<VideoFrame> InterprocessFramePool::ResurrectLastVideoFrame(
    VideoPixelFormat expected_format,
    const gfx::Size& expected_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Find the tracking entry for the resurrectable buffer. If it is still being
  // used, or is not of the expected format and size, punt.
  if (resurrectable_buffer_memory_ == nullptr ||
      last_delivered_format_ != expected_format ||
      last_delivered_size_ != expected_size) {
    return nullptr;
  }
  const auto it = std::find_if(
      available_buffers_.rbegin(), available_buffers_.rend(),
      [this](const PooledBuffer& candidate) {
        return candidate.mapping.memory() == resurrectable_buffer_memory_;
      });
  if (it == available_buffers_.rend()) {
    return nullptr;
  }

  // Wrap the buffer in a VideoFrame and return it.
  PooledBuffer resurrected = std::move(*it);
  available_buffers_.erase(it.base() - 1);
  return WrapBuffer(std::move(resurrected), expected_format, expected_size);
}

base::ReadOnlySharedMemoryRegion InterprocessFramePool::CloneHandleForDelivery(
    const VideoFrame* frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Record that this frame is the last-delivered one, for possible future calls
  // to ResurrectLastVideoFrame().
  const auto it = utilized_buffers_.find(frame);
  DCHECK(it != utilized_buffers_.end());
  // Assumption: The first image plane's memory pointer should be the start of
  // the writable mapped memory. WrapBuffer() sanity-checks this.
  resurrectable_buffer_memory_ = frame->data(0);
  last_delivered_format_ = frame->format();
  last_delivered_size_ = frame->coded_size();

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
  // Sanity-check the assumption being made in CloneHandleForDelivery():
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
