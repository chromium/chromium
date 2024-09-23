// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_VIDEO_CAPTURE_CLIENT_H_
#define COMPONENTS_MIRRORING_SERVICE_VIDEO_CAPTURE_CLIENT_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/video_frame_converter.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace media {
class VideoFrame;
class VideoFramePool;
struct VideoCaptureFeedback;
}  // namespace media

namespace mirroring {

// On Start(), this class connects to |host| through the
// media::mojom::VideoCaptureHost interface and requests to launch a video
// capture device. After the device is started, the captured video frames are
// received through the media::mojom::VideoCaptureObserver interface.
class COMPONENT_EXPORT(MIRRORING_SERVICE) VideoCaptureClient
    : public media::mojom::VideoCaptureObserver {
 public:
  VideoCaptureClient(const media::VideoCaptureParams& params,
                     mojo::PendingRemote<media::mojom::VideoCaptureHost> host);

  VideoCaptureClient(const VideoCaptureClient&) = delete;
  VideoCaptureClient& operator=(const VideoCaptureClient&) = delete;

  ~VideoCaptureClient() override;

  using FrameDeliverCallback = base::RepeatingCallback<void(
      scoped_refptr<media::VideoFrame> video_frame)>;
  void Start(FrameDeliverCallback deliver_callback,
             base::OnceClosure error_callback);

  void Stop();

  // Will stop delivering frames on this call.
  void Pause();

  void Resume(FrameDeliverCallback deliver_callback);

  // Feedback callback.
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback);

  // Requests to receive a refreshed captured video frame. Do nothing if the
  // capturing device is not started or the capturing is paused.
  void RequestRefreshFrame();

  // media::mojom::VideoCaptureObserver implementations.
  void OnStateChanged(media::mojom::VideoCaptureResultPtr result) override;
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnBufferReady(media::mojom::ReadyBufferPtr buffer) override;
  void OnBufferDestroyed(int32_t buffer_id) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;

  void SwitchVideoCaptureHost(
      mojo::PendingRemote<media::mojom::VideoCaptureHost> host);

 private:
  using BufferFinishedCallback = base::OnceCallback<void()>;
  // Called by the VideoFrame destructor.
  static void DidFinishConsumingFrame(BufferFinishedCallback callback);

  // Reports the utilization to release the buffer for potential reuse.
  using MappingKeepAlive = absl::variant<absl::monostate,
                                         base::WritableSharedMemoryMapping,
                                         base::ReadOnlySharedMemoryMapping>;
  void OnClientBufferFinished(int buffer_id,
                              MappingKeepAlive mapping_keep_alive);

  const media::VideoCaptureParams params_;
  mojo::Remote<media::mojom::VideoCaptureHost> video_capture_host_;

  // Called when capturing failed to start.
  base::OnceClosure error_callback_;

  mojo::Receiver<media::mojom::VideoCaptureObserver> receiver_{this};

  // TODO(crbug.com/40576409): Store the base::ReadOnlySharedMemoryRegion
  // instead after migrating the media::VideoCaptureDeviceClient to the new
  // shared memory API.
  using ClientBufferMap =
      base::flat_map<int32_t, media::mojom::VideoBufferHandlePtr>;
  // Stores the buffer handler on OnBufferCreated(). |buffer_id| is the key.
  ClientBufferMap client_buffers_;

  // The reference time for the first frame. Used to calculate the timestamp of
  // the captured frame if not provided in the frame info.
  base::TimeTicks first_frame_ref_time_;

  // The callback to deliver the received frame.
  FrameDeliverCallback frame_deliver_callback_;

  // Latest received feedback.
  media::VideoCaptureFeedback feedback_;

  // Cast Streaming does not support NV12 frames. When NV12 frames are received,
  // these structures are used to convert them to I420 on the CPU.
  // https://crbug.com/1206325
  std::unique_ptr<media::VideoFramePool> nv12_to_i420_pool_;
  media::VideoFrameConverter frame_converter_;

  // Indicates whether we're in the middle of switching video capture host.
  bool switching_video_capture_host_ = false;

  // Represents the timestamp for the last frame sent to be delivered through
  // `frame_deliver_callback_`.
  base::TimeDelta last_timestamp_;

  // When capturing stops, it gets assigned the value of the `last_timestamp_`.
  // Added to frame timestamps when capturing restarts, since frame
  // timestamps are expected to always increase.
  base::TimeDelta accumulated_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoCaptureClient> weak_factory_{this};
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_VIDEO_CAPTURE_CLIENT_H_
