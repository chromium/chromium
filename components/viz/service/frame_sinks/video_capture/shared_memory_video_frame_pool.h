// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_SHARED_MEMORY_VIDEO_FRAME_POOL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_SHARED_MEMORY_VIDEO_FRAME_POOL_H_

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/viz/service/frame_sinks/video_capture/video_frame_pool.h"
#include "components/viz/service/viz_service_export.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

// A pool of VideoFrames backed by shared memory that can be transported
// efficiently across mojo service boundaries.
class VIZ_SERVICE_EXPORT SharedMemoryVideoFramePool : public VideoFramePool {
 public:
  // |capacity| is the maximum number of pooled VideoFrames; but they can be of
  // any byte size.
  explicit SharedMemoryVideoFramePool(int capacity);

  SharedMemoryVideoFramePool(const SharedMemoryVideoFramePool&) = delete;
  SharedMemoryVideoFramePool& operator=(const SharedMemoryVideoFramePool&) =
      delete;

  ~SharedMemoryVideoFramePool() override;

  scoped_refptr<media::VideoFrame> ReserveVideoFrame(
      media::VideoPixelFormat format,
      const gfx::Size& size) override;

  media::mojom::VideoBufferHandlePtr CloneHandleForDelivery(
      const media::VideoFrame& frame) override;

  size_t GetNumberOfReservedFrames() const override;

 private:
  using PooledBuffer = base::MappedReadOnlyRegion;

  // Creates a media::VideoFrame backed by a specific pooled buffer.
  scoped_refptr<media::VideoFrame> WrapBuffer(PooledBuffer pooled_buffer,
                                              media::VideoPixelFormat format,
                                              const gfx::Size& size);

  // Called when a VideoFrame goes out of scope, to remove the entry from
  // |utilized_buffers_| and place the PooledBuffer back into
  // |available_buffers_|.
  void OnFrameWrapperDestroyed(const media::VideoFrame* frame,
                               base::WritableSharedMemoryMapping mapping);

  // Returns true if a shared memory failure can be logged. This is a rate
  // throttle, to ensure the logs aren't spammed in chronically low-memory
  // environments.
  bool CanLogSharedMemoryFailure();

  // Buffers available for immediate re-use. Generally, it is best to push and
  // pop from the back of this vector so that the most-recently-used buffers are
  // re-used. This will help prevent excessive operating system paging in low-
  // memory situations.
  std::vector<PooledBuffer> available_buffers_;

  // A map of externally-owned VideoFrames and the tracking information about
  // the shared memory buffer backing them.
  base::flat_map<const media::VideoFrame*, base::ReadOnlySharedMemoryRegion>
      utilized_buffers_;

  // The time at which the last shared memory allocation or mapping failed.
  base::TimeTicks last_fail_log_time_;

  // The amount of time that should have elapsed between log warnings about
  // shared memory allocation/mapping failures.
  static constexpr base::TimeDelta kMinLoggingPeriod = base::Seconds(10);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SharedMemoryVideoFramePool> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_SHARED_MEMORY_VIDEO_FRAME_POOL_H_
