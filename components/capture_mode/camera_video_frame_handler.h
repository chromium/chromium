// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTURE_MODE_CAMERA_VIDEO_FRAME_HANDLER_H_
#define COMPONENTS_CAPTURE_MODE_CAMERA_VIDEO_FRAME_HANDLER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/capture_mode/capture_mode_export.h"
#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace ui {
class ContextFactory;
}  // namespace ui

namespace capture_mode {

// Defines an interface for an object that can hold and own a video buffer
// handle, and able to extract a `VideoFrame` from the buffer when the frame
// becomes ready in it. Concrete subclasses implements this frame extraction
// differently based on the type of the buffer.
class BufferHandleHolder {
 public:
  BufferHandleHolder(const BufferHandleHolder&) = delete;
  BufferHandleHolder& operator=(const BufferHandleHolder&) = delete;
  virtual ~BufferHandleHolder();

  // Creates and returns a concrete implementation of this interface that
  // matches the buffer type of the given `buffer_handle`.
  // We only support the `kGpuMemoryBuffer` and `kSharedMemory` buffer types.
  // `require_mapped_frame_callback` is a callback that is passed to the buffer
  // handle only on Windows. It is used to request pre-mapped frames.
  static std::unique_ptr<BufferHandleHolder> Create(
      media::mojom::VideoBufferHandlePtr buffer_handle,
      ui::ContextFactory* context_factory,
      base::RepeatingClosure require_mapped_frame_callback);

  // Extracts and returns the ready video frame in the given `buffer`.
  virtual scoped_refptr<media::VideoFrame> OnFrameReadyInBuffer(
      video_capture::mojom::ReadyFrameInBufferPtr buffer) = 0;

 protected:
  BufferHandleHolder() = default;
};

// -----------------------------------------------------------------------------

// Defines an object that will subscribe to a camera device, whose remote video
// source is the given `camera_video_source`. It will handle the reception of
// the video frames from that device and provide them to its `Delegate`.
class CAPTURE_MODE_EXPORT CameraVideoFrameHandler
    : public video_capture::mojom::VideoFrameHandler {
 public:
  // Defines an interface for a delegate of this class, which will be provided
  // by the video frames received from the camera device.
  class Delegate {
   public:
    // Will be called on the UI thread whenever a video `frame` is received from
    // the camera device.
    virtual void OnCameraVideoFrame(scoped_refptr<media::VideoFrame> frame) = 0;

    // Called when the handler received a fatal error in `OnError()` or the mojo
    // remote to the `VideoSource` gets disconnected.
    virtual void OnFatalErrorOrDisconnection() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Creates an instance of this class which will subscribe to the given
  // `camera_video_source` requesting to receive video frames of its feed with
  // the given `capture_format`.
  // `device_id` represents the id of the camera providing the video / frames.
  // It is a default parameter, because it is only needed on Mac to decide if we
  // can use kGpuMemoryBuffer.
  CameraVideoFrameHandler(
      ui::ContextFactory* context_factory,
      mojo::Remote<video_capture::mojom::VideoSource> camera_video_source,
      const media::VideoCaptureFormat& capture_format,
      const std::string& device_id = std::string());
  CameraVideoFrameHandler(const CameraVideoFrameHandler&) = delete;
  CameraVideoFrameHandler& operator=(const CameraVideoFrameHandler&) = delete;
  ~CameraVideoFrameHandler() override;

  // Activates the subscription so we start receiving video frames. The video
  // frames will then be provided to the `delegate`.
  void StartHandlingFrames(Delegate* delegate);

  // Closes the camera video stream subscription and immediately rejects any
  // new frames received at `OnFrameReadyInBuffer()`. The callback is invoked
  // once the mojo call to close the subscription is complete, at which point
  // it is guaranteed that no more calls will be made to the
  // `VideoFrameHandler`.
  //
  // The intended usage of this method is for the caller to guarantee that the
  // instance of `VideoFrameHandler` lives until the `Close()` call is
  // complete by binding it to the `close_complete_callback`. This ensures
  // that any buffers allocated between calling `Close()` and having the mojo
  // serviced can be appropriately released. If the caller doesn't take this
  // step and just deletes the `VideoFrameHandler` then it's possible for
  // buffers to be allocated and held in limbo until the video stream is
  // stopped.
  //
  // `delegate_` is nullified by this method so it is safe to free the memory
  // backing `delegate_` any time after this point.
  void Close(base::OnceClosure close_complete_callback);

  // video_capture::mojom::VideoFrameHandler:
  void OnCaptureConfigurationChanged() override;
  void OnNewBuffer(int buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameAccessHandlerReady(
      mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
          pending_frame_access_handler) override;
  void OnFrameReadyInBuffer(
      video_capture::mojom::ReadyFrameInBufferPtr buffer) override;
  void OnBufferRetired(int buffer_id) override;
  void OnError(media::VideoCaptureError error) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override;
  void OnFrameWithEmptyRegionCapture() override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;
  void OnStopped() override;

  // The `kGpuMemoryBuffer` type is requested only when running on an actual
  // device. This allows force-requesting them even when running on
  // linux-chromeos for unit testing purposes.
  static void SetForceUseGpuMemoryBufferForTest(bool value);

  const std::optional<media::VideoCaptureParams>& GetActualParams() const {
    return actual_params_;
  }

 private:
  void OnSubscriptionCreationResult(
      video_capture::mojom::CreatePushSubscriptionResultCodePtr result_code,
      const media::VideoCaptureParams& actual_params);

  // Called when a video frame is destroyed, which was backed by a buffer whose
  // ID is the given `buffer_id`. This lets us inform the video capture
  // service's `VideoFrameAccessHandler` that we're done consuming this buffer
  // so it can be reused again by the video frames producer.
  void OnVideoFrameGone(int buffer_id);

  // Called when a fatal error is reported in `OnError()` or the mojo remote to
  // `VideoSource` gets disconnected.
  void OnFatalErrorOrDisconnection();

  // Sends a ProcessFeedback() call to `camera_video_stream_subsciption_remote_`
  // requesting mapped frames. Only used on Windows.
  void RequireMappedFrame();

  raw_ptr<Delegate> delegate_ = nullptr;

  raw_ptr<ui::ContextFactory> context_factory_;

  // Determines if new buffers should be processed. If false, any newly received
  // buffers are immediately released.
  bool active_ = false;

  mojo::Receiver<video_capture::mojom::VideoFrameHandler>
      video_frame_handler_receiver_{this};

  // A remote bound to the camera's `VideoSource` implementation in the video
  // capture service.
  mojo::Remote<video_capture::mojom::VideoSource> camera_video_source_remote_;

  // A remote bound to the `PushVideoStreamSubscription` implementation in the
  // video capture service. Once this subscription is activated, we start
  // receiving video frames.
  mojo::Remote<video_capture::mojom::PushVideoStreamSubscription>
      camera_video_stream_subsciption_remote_;

  // A remote bound to the `VideoFrameAccessHandler` implementation in the video
  // capture service, which is used to signal that we're done consuming a buffer
  // so that the buffer can be reused by the frames producer. This is stored as
  // a SharedRemote so that frames deleted after destruction of this class can
  // still be released by the instance of the SharedRemote captured in the frame
  // destruction callback.
  mojo::SharedRemote<video_capture::mojom::VideoFrameAccessHandler>
      video_frame_access_handler_remote_;

  // Maps the `BufferHandleHolder`s by the `buffer_id`. An entry is inserted
  // when `OnNewBuffer()` is called, and removed when `OnBufferRetired()` is
  // called.
  base::flat_map</*buffer_id=*/int, std::unique_ptr<BufferHandleHolder>>
      buffer_map_;

  // Represents the actual params that the source was opened with.
  std::optional<media::VideoCaptureParams> actual_params_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CameraVideoFrameHandler> weak_ptr_factory_{this};
};

}  // namespace capture_mode

#endif  // COMPONENTS_CAPTURE_MODE_CAMERA_VIDEO_FRAME_HANDLER_H_
