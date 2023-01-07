// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_VIDEO_FRAME_POOL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_VIDEO_FRAME_POOL_H_

#include "base/memory/scoped_refptr.h"
#include "components/viz/service/viz_service_export.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoFrame;
}

namespace viz {

// A pool of VideoFrames that can be transported efficiently across mojo service
// boundaries. The underlying backing for VideoFrames depends on the concrete
// implementation.
class VIZ_SERVICE_EXPORT VideoFramePool {
 public:
  VideoFramePool(const VideoFramePool&) = delete;
  VideoFramePool(VideoFramePool&&) = delete;
  VideoFramePool& operator=(const VideoFramePool&) = delete;
  VideoFramePool& operator=(VideoFramePool&&) = delete;

  virtual ~VideoFramePool();

  // Reserves a buffer from the pool and creates a VideoFrame backed by it.
  // When the ref-count of the returned VideoFrame goes to zero, the
  // reservation is released and the frame becomes available for reuse. Returns
  // null if the pool is fully utilized or if the frame couldn't have been
  // created for another reason.
  virtual scoped_refptr<media::VideoFrame> ReserveVideoFrame(
      media::VideoPixelFormat format,
      const gfx::Size& size) = 0;

  // Returns a handle to |frame|'s backing. Note that the client should
  // not allow the ref-count of the passed-in VideoFrame to reach zero until
  // downstream consumers are finished using it, as this would allow the
  // underlying backing to be reused.
  virtual media::mojom::VideoBufferHandlePtr CloneHandleForDelivery(
      const media::VideoFrame& frame) = 0;

  // Returns the current pool utilization, based on the number of VideoFrames
  // being held by the client.
  float GetUtilization() const;

  // Returns the number of frames that are currently reserved (and thus still
  // owned by the caller of |ReserveVideoFrame()|).
  virtual size_t GetNumberOfReservedFrames() const = 0;

  size_t capacity() const { return capacity_; }

 protected:
  // |capacity| is the maximum number of pooled VideoFrames; but they can be of
  // any byte size.
  explicit VideoFramePool(int capacity);

 private:
  // The maximum number of buffers. However, the buffers themselves can be of
  // any byte size.
  const size_t capacity_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_VIDEO_FRAME_POOL_H_
