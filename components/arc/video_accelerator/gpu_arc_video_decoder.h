// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODER_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODER_H_

#include <map>
#include <memory>
#include <queue>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/arc/mojom/video_decode_accelerator.mojom.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder.h"
#include "media/gpu/chromeos/vda_video_frame_pool.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {

class ProtectedBufferManager;

// The GpuArcVideoDecoder listens to mojo IPC requests and forwards these to an
// instance of media::VideoDecoder. Decoded frames are passed back to the mojo
// client.
//
// Note: Even though currently the underlying decoder uses the
// media::VideoDecoder interface, the mojo communication with the client still
// uses the mojo::VideoDecodeAccelerator interface. A new mojo::VideoDecoder
// interface will be introduced in a future step.
// TODO(dstaessens): Switch to a new VD-based mojo client.
class GpuArcVideoDecoder : public mojom::VideoDecodeAccelerator,
                           public media::VdaVideoFramePool::VdaDelegate {
 public:
  GpuArcVideoDecoder(
      scoped_refptr<ProtectedBufferManager> protected_buffer_manager);
  ~GpuArcVideoDecoder() override;

  GpuArcVideoDecoder(const GpuArcVideoDecoder&) = delete;
  GpuArcVideoDecoder& operator=(const GpuArcVideoDecoder&) = delete;
  GpuArcVideoDecoder(GpuArcVideoDecoder&&) = delete;
  GpuArcVideoDecoder& operator=(GpuArcVideoDecoder&&) = delete;

  // Implementation of mojom::VideoDecodeAccelerator.
  void Initialize(mojom::VideoDecodeAcceleratorConfigPtr config,
                  mojo::PendingRemote<mojom::VideoDecodeClient> client,
                  InitializeCallback callback) override;
  void Decode(mojom::BitstreamBufferPtr bitstream_buffer) override;
  void AssignPictureBuffers(uint32_t count) override;
  void ImportBufferForPicture(int32_t picture_buffer_id,
                              mojom::HalPixelFormat format,
                              mojo::ScopedHandle handle,
                              std::vector<VideoFramePlane> planes,
                              mojom::BufferModifierPtr modifier) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush(FlushCallback callback) override;
  void Reset(ResetCallback callback) override;

  // Implementation of VdaVideoFramePool::VdaDelegate.
  void RequestFrames(const media::Fourcc& fourcc,
                     const gfx::Size& coded_size,
                     const gfx::Rect& visible_rect,
                     size_t max_num_frames,
                     NotifyLayoutChangedCb notify_layout_changed_cb,
                     ImportFrameCb import_frame_cb) override;

  // Called when all references to a video frame have been dropped.
  void OnFrameReleased(scoped_refptr<media::VideoFrame> origin_frame);

 private:
  // Buffer allocation flow (used on initialization and resolution changes):
  // 1. The VdaVideoFramePool calls RequestFrames() to request N buffers. The
  //    request is forwarded to the mojo client using ProvidePictureBuffers().
  // 2. The mojo client calls AssignPictureBuffers().
  // 3. The mojo client calls ImportBufferForPicture() N times.
  // 4. After sending a decoded frame to the client using PictureReady(), the
  //    client calls ReusePictureBuffer() when the buffer can be recycled.
  enum class DecoderState {
    kUninitialized,                 // The decoder is not initialized yet
    kAwaitingAssignPictureBuffers,  // Waiting for AssignPictureBuffers()
    kAwaitingFirstImport,           // Wait for ImportBufferForPicture()
    kDecoding,                      // Ready to decode requests
    kError                          // An error occurred
  };

  using Request = base::OnceClosure;
  using DmabufId = media::DmabufVideoFramePool::DmabufId;

  // Called by the decoder when initialization is done.
  void OnInitializeDone(media::Status status);
  // Called by the decoder when the specified buffer has been decoded.
  void OnDecodeDone(int32_t bitstream_buffer_id, media::Status status);
  // Called by the decoder when a decoded frame is ready.
  void OnFrameReady(scoped_refptr<media::VideoFrame> frame);
  // Called by the decoder when a flush request has been completed.
  void OnFlushDone(media::Status status);
  // Called by the decoder when a reset request has been completed.
  void OnResetDone();
  // Called when an error occurred.
  void OnError(base::Location location, Result error);

  // Report the initialization status to the client.
  void HandleInitializeDone(Result result);
  // Handle all requests that are currently in the |requests_| queue.
  void HandleRequests();
  // Handle the specified request. If the decoder is currently resetting the
  // request will be queued and handled once OnResetDone() is called.
  void HandleRequest(Request request);
  // Handle a decode request of the specified |buffer|.
  void HandleDecodeRequest(int bitstream_id,
                           scoped_refptr<media::DecoderBuffer> buffer);
  // Handle a flush request with specified |callback|. Multiple simultaneous
  // flush requests are allowed and will be handled in FIFO order.
  void HandleFlushRequest(FlushCallback callback);
  // Handle a reset request with specified |callback|. All ongoing flush
  // operations will be reported as canceled.
  void HandleResetRequest(ResetCallback callback);

  // Create a decoder buffer from the specified |fd|.
  scoped_refptr<media::DecoderBuffer> CreateDecoderBuffer(base::ScopedFD fd,
                                                          uint32_t offset,
                                                          uint32_t bytes_used);
  // Create a GPU memory handle from the specified |fd|.
  gfx::GpuMemoryBufferHandle CreateGpuMemoryHandle(
      base::ScopedFD fd,
      const std::vector<VideoFramePlane>& planes,
      media::VideoPixelFormat pixel_format,
      uint64_t modifier);
  // Create a video frame from the specified |gmb_handle|.
  scoped_refptr<media::VideoFrame> CreateVideoFrame(
      gfx::GpuMemoryBufferHandle gmb_handle,
      media::VideoPixelFormat pixel_format) const;

  // The number of currently active instances. Always accessed on the same
  // thread, so we don't need to use a lock.
  static size_t num_instances_;

  // Current state of the video decoder.
  DecoderState decoder_state_ = DecoderState::kUninitialized;

  // The remote mojo client.
  mojo::Remote<mojom::VideoDecodeClient> client_;
  // The local video decoder.
  std::unique_ptr<media::VideoDecoder> decoder_;

  // callback used to notify the video frame pool of new video frame formats,
  // used when the pool requests new frames using RequestFrames().
  NotifyLayoutChangedCb notify_layout_changed_cb_;
  // Callback used to insert new frames in the video frame pool.
  ImportFrameCb import_frame_cb_;
  // Initialization callback, used while the decoder is being initialized.
  InitializeCallback init_callback_;
  // The list of callbacks associated with any in-progress flush requests.
  std::queue<FlushCallback> flush_callbacks_;
  // The callback associated with the ongoing reset request if any.
  ResetCallback reset_callback_;
  // Requests currently waiting until resetting the decoder has completed.
  std::queue<Request> requests_;

  // The coded size currently used for video frames.
  gfx::Size coded_size_;
  // The coded size requested by the decoder, will be applied once the Mojo
  // client called AssignPictureBuffers().
  gfx::Size requested_coded_size_;
  // The current video frame layout.
  absl::optional<media::VideoFrameLayout> video_frame_layout_;
  // The number of output buffers.
  size_t output_buffer_count_ = 0;

  // The video frames currently in use by the client and their associated
  // picture buffer ids. We need to hold references to video frames to prevent
  // them from being returned to the video frame pool for reuse while they are
  // still in use by our client. The use count is tracked as the same frame
  // might be sent multiple times to the client when using the VP9
  // 'show_existing_frame' feature.
  std::map<int32_t, std::pair<scoped_refptr<media::VideoFrame>, size_t>>
      client_video_frames_;
  // Map of video frame's DmabufId to the associated picture buffer id.
  std::map<DmabufId, int32_t> frame_id_to_picture_id_;

  // The protected buffer manager, used when decoding an encrypted video.
  scoped_refptr<ProtectedBufferManager> protected_buffer_manager_;
  // Whether we're decoding an encrypted video.
  absl::optional<bool> secure_mode_;

  // The client task runner and its sequence checker. All methods should be run
  // on this task runner.
  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<GpuArcVideoDecoder> weak_this_;
  base::WeakPtrFactory<GpuArcVideoDecoder> weak_this_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_GPU_ARC_VIDEO_DECODER_H_
