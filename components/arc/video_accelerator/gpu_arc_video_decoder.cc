// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/gpu_arc_video_decoder.h"

#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "components/arc/video_accelerator/protected_buffer_manager.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#include "media/gpu/macros.h"

namespace arc {

namespace {

// Heuristically chosen maximum number of output buffers that the untrusted
// client is allowed to request, to prevent it from allocating too much memory.
constexpr size_t kMaxOutputBufferCount = 32;

// Heuristically chosen maximum number of concurrent decoder instances, as
// system resources are limited.
constexpr size_t kMaxConcurrentInstances = 8;

// Convert the specified |bitstream_id| to a timestamp, which we can use to
// identify which bitstream buffer a decoded video frame belongs to.
base::TimeDelta BitstreamIdToFakeTimestamp(int32_t bitstream_id) {
  return base::Milliseconds(bitstream_id);
}

// Convert the specified |timestamp| to a bitstream id, so we can map between
// video frames and bitstream buffers.
int32_t FakeTimestampToBitstreamId(base::TimeDelta timestamp) {
  return static_cast<int32_t>(timestamp.InMilliseconds());
}

// Helper thunk called when all references to a video frame have been dropped,
// required as weak pointers should only be accessed on the creator thread.
void OnFrameReleasedThunk(
    absl::optional<base::WeakPtr<GpuArcVideoDecoder>> weak_this,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<media::VideoFrame> origin_frame) {
  DCHECK(weak_this);
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&GpuArcVideoDecoder::OnFrameReleased,
                                       *weak_this, std::move(origin_frame)));
}

// Get the video pixel format associated with the specified mojo pixel |format|.
media::VideoPixelFormat GetPixelFormat(mojom::HalPixelFormat format) {
  switch (format) {
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YV12:
      return media::PIXEL_FORMAT_YV12;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_NV12:
      return media::PIXEL_FORMAT_NV12;
    default:
      return media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN;
  }
}

}  // namespace

// static
size_t GpuArcVideoDecoder::num_instances_ = 0;

GpuArcVideoDecoder::GpuArcVideoDecoder(
    scoped_refptr<ProtectedBufferManager> protected_buffer_manager)
    : protected_buffer_manager_(std::move(protected_buffer_manager)) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

GpuArcVideoDecoder::~GpuArcVideoDecoder() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invalidate all weak pointers to stop incoming callbacks.
  weak_this_factory_.InvalidateWeakPtrs();

  // The number of active instances should always be larger than zero. But if a
  // bug causes an underflow we will permanently be unable to create new
  // decoders, so an extra check is performed here (see b/173700103).
  if (decoder_ && num_instances_ > 0) {
    num_instances_--;
  }
  decoder_.reset();

  // The VdaVideoFramePool is blocked until this callback is executed, so we
  // need to make sure to call it before destroying.
  if (notify_layout_changed_cb_) {
    std::move(notify_layout_changed_cb_).Run(absl::nullopt);
  }
}

void GpuArcVideoDecoder::Initialize(
    mojom::VideoDecodeAcceleratorConfigPtr config,
    mojo::PendingRemote<mojom::VideoDecodeClient> client,
    InitializeCallback callback) {
  VLOGF(2) << "profile: " << config->profile
           << ", secure mode: " << config->secure_mode;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(decoder_state_, DecoderState::kUninitialized);
  DCHECK(!client_ && !init_callback_);

  client_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  client_.Bind(std::move(client));
  init_callback_ = std::move(callback);

  if (decoder_) {
    VLOGF(1) << "Re-initialization is not allowed";
    HandleInitializeDone(Result::ILLEGAL_STATE);
    return;
  }

  if (num_instances_ >= kMaxConcurrentInstances) {
    VLOGF(1) << "Maximum concurrent instances reached: " << num_instances_;
    HandleInitializeDone(Result::INSUFFICIENT_RESOURCES);
    return;
  }

  decoder_ = media::VideoDecoderPipeline::Create(
      client_task_runner_,
      std::make_unique<media::VdaVideoFramePool>(weak_this_,
                                                 client_task_runner_),
      std::make_unique<media::VideoFrameConverter>(),
      std::make_unique<media::NullMediaLog>());

  if (!decoder_) {
    VLOGF(1) << "Failed to create video decoder";
    HandleInitializeDone(Result::PLATFORM_FAILURE);
    return;
  }
  num_instances_++;

  // We only know the size after the first decoder buffer has been queued and
  // the decoder calls RequestFrames(), so we use an arbitrary size as default.
  const gfx::Size initial_coded_size = gfx::Size(320, 240);
  media::VideoDecoderConfig vd_config(
      media::VideoCodecProfileToVideoCodec(config->profile), config->profile,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::kNoTransformation, initial_coded_size,
      gfx::Rect(initial_coded_size), initial_coded_size, std::vector<uint8_t>(),
      media::EncryptionScheme::kUnencrypted);
  auto init_cb =
      base::BindOnce(&GpuArcVideoDecoder::OnInitializeDone, weak_this_);
  auto output_cb =
      base::BindRepeating(&GpuArcVideoDecoder::OnFrameReady, weak_this_);

  decoder_->Initialize(std::move(vd_config), false, nullptr, std::move(init_cb),
                       std::move(output_cb), media::WaitingCB());
  VLOGF(2) << "Number of concurrent decoder instances: " << num_instances_;
}

void GpuArcVideoDecoder::Decode(mojom::BitstreamBufferPtr bitstream_buffer) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (decoder_state_ == DecoderState::kError) {
    return;
  }

  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    return;
  }

  // Get the buffer fd from the bitstream buffer.
  base::ScopedFD fd =
      UnwrapFdFromMojoHandle(std::move(bitstream_buffer->handle_fd));
  if (!fd.is_valid()) {
    OnError(FROM_HERE, Result::INVALID_ARGUMENT);
    return;
  }
  DVLOGF(4) << "bitstream buffer id: " << bitstream_buffer->bitstream_id
            << ", fd: " << fd.get();

  // If this is the first input buffer, determine if the playback is secure.
  if (!secure_mode_.has_value()) {
    if (protected_buffer_manager_) {
      secure_mode_ = IsBufferSecure(protected_buffer_manager_.get(), fd);
      VLOGF(2) << "First input buffer secure: " << *secure_mode_;
    } else {
      secure_mode_ = false;
      DVLOGF(3) << "No protected buffer manager, treating playback as normal";
    }
  }

  scoped_refptr<media::DecoderBuffer> decoder_buffer = CreateDecoderBuffer(
      std::move(fd), bitstream_buffer->offset, bitstream_buffer->bytes_used);
  if (!decoder_buffer) {
    VLOGF(1) << "Failed to create decoder buffer from fd";
    OnError(FROM_HERE, Result::INVALID_ARGUMENT);
    return;
  }

  // Use the bitstream buffer's id as timestamp, so we can later identify which
  // bitstream buffer a decoded video frame belongs to in OnFrameReady().
  decoder_buffer->set_timestamp(
      BitstreamIdToFakeTimestamp(bitstream_buffer->bitstream_id));

  // Using unretained is safe here, all callbacks are guaranteed to be executed
  // before the decoder is destroyed.
  HandleRequest(base::BindOnce(
      &GpuArcVideoDecoder::HandleDecodeRequest, base::Unretained(this),
      bitstream_buffer->bitstream_id, std::move(decoder_buffer)));
}

void GpuArcVideoDecoder::AssignPictureBuffers(uint32_t count) {
  VLOGF(2) << "count: " << count;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    return;
  }
  if (count > kMaxOutputBufferCount) {
    VLOGF(1) << "Too many output buffers requested: %u " << count;
    OnError(FROM_HERE, Result::INVALID_ARGUMENT);
    return;
  }
  if (decoder_state_ != DecoderState::kAwaitingAssignPictureBuffers) {
    VLOGF(1) << "AssignPictureBuffers called in wrong state";
    OnError(FROM_HERE, Result::INVALID_ARGUMENT);
    return;
  }

  coded_size_ = requested_coded_size_;
  output_buffer_count_ = count;
  decoder_state_ = DecoderState::kAwaitingFirstImport;
}

void GpuArcVideoDecoder::ImportBufferForPicture(
    int32_t picture_buffer_id,
    mojom::HalPixelFormat format,
    mojo::ScopedHandle handle,
    std::vector<VideoFramePlane> planes,
    mojom::BufferModifierPtr modifier_ptr) {
  DVLOGF(3) << "id: " << picture_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    return;
  }
  if (decoder_state_ != DecoderState::kAwaitingFirstImport &&
      decoder_state_ != DecoderState::kDecoding) {
    VLOGF(1) << "ImportBufferForPicture called in wrong state, ignoring";
    return;
  }

  if ((picture_buffer_id < 0) ||
      (static_cast<size_t>(picture_buffer_id) >= output_buffer_count_)) {
    VLOGF(1) << "Invalid picture buffer id: " << picture_buffer_id;
    OnError(FROM_HERE, Result::INVALID_ARGUMENT);
    return;
  }

  media::VideoPixelFormat pixel_format = GetPixelFormat(format);
  if (pixel_format == media::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
    VLOGF(1) << "Unsupported format: " << format;
    OnError(FROM_HERE, Result::INVALID_ARGUMENT);
    return;
  }

  // Convert the Mojo buffer fd to a GpuMemoryBufferHandle.
  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(handle));
  if (!fd.is_valid()) {
    OnError(FROM_HERE, Result::INVALID_ARGUMENT);
    return;
  }
  const uint64_t modifier =
      modifier_ptr ? modifier_ptr->val : gfx::NativePixmapHandle::kNoModifier;
  gfx::GpuMemoryBufferHandle gmb_handle =
      CreateGpuMemoryHandle(std::move(fd), planes, pixel_format, modifier);
  if (gmb_handle.is_null()) {
    VLOGF(1) << "Failed to create GPU memory handle from fd";
    OnError(FROM_HERE, Result::INVALID_ARGUMENT);
    return;
  }

  // If this is the first imported buffer after requesting new video frames we
  // need to update the video frame layout.
  if (decoder_state_ == DecoderState::kAwaitingFirstImport) {
    DCHECK(notify_layout_changed_cb_);

    const uint64_t layout_modifier =
        (gmb_handle.type == gfx::NATIVE_PIXMAP)
            ? gmb_handle.native_pixmap_handle.modifier
            : gfx::NativePixmapHandle::kNoModifier;
    std::vector<media::ColorPlaneLayout> color_planes;
    for (const auto& plane : gmb_handle.native_pixmap_handle.planes) {
      color_planes.emplace_back(plane.stride, plane.offset, plane.size);
    }
    video_frame_layout_ = media::VideoFrameLayout::CreateWithPlanes(
        pixel_format, coded_size_, color_planes,
        media::VideoFrameLayout::kBufferAddressAlignment, layout_modifier);
    if (!video_frame_layout_) {
      VLOGF(1) << "Failed to create VideoFrameLayout";
      OnError(FROM_HERE, Result::PLATFORM_FAILURE);
      return;
    }

    // Notify the VdaVideoFramePool that the layout changed.
    auto fourcc = media::Fourcc::FromVideoPixelFormat(pixel_format);
    if (!fourcc) {
      VLOGF(1) << "Failed to convert to fourcc";
      OnError(FROM_HERE, Result::PLATFORM_FAILURE);
      return;
    }
    std::move(notify_layout_changed_cb_)
        .Run(media::GpuBufferLayout::Create(*fourcc, coded_size_, color_planes,
                                            layout_modifier));

    decoder_state_ = DecoderState::kDecoding;
  }

  scoped_refptr<media::VideoFrame> origin_frame =
      CreateVideoFrame(std::move(gmb_handle), pixel_format);
  if (!origin_frame) {
    VLOGF(1) << "Failed to create video frame from fd";
    OnError(FROM_HERE, Result::INVALID_ARGUMENT);
    return;
  }

  DmabufId dmabuf_id = media::DmabufVideoFramePool::GetDmabufId(*origin_frame);
  auto it = frame_id_to_picture_id_.emplace(dmabuf_id, picture_buffer_id);
  DCHECK(it.second);

  // Wrap the video frame and attach a destruction observer so we're notified
  // when the video frame can be returned to the pool.
  scoped_refptr<media::VideoFrame> wrapped_frame =
      media::VideoFrame::WrapVideoFrame(origin_frame, origin_frame->format(),
                                        origin_frame->visible_rect(),
                                        origin_frame->natural_size());
  wrapped_frame->AddDestructionObserver(
      base::BindOnce(&OnFrameReleasedThunk, weak_this_, client_task_runner_,
                     std::move(origin_frame)));

  // When importing a picture it shouldn't already be in the video frames map.
  // Remove it as an extra safety check to make sure the usecount remains valid.
  if (client_video_frames_.erase(picture_buffer_id) > 0) {
    VLOGF(1) << "Dropped existing reference to picture: " << picture_buffer_id;
  }

  DCHECK(import_frame_cb_);
  import_frame_cb_.Run(std::move(wrapped_frame));
}

void GpuArcVideoDecoder::ReusePictureBuffer(int32_t picture_buffer_id) {
  DVLOGF(4) << "id: " << picture_buffer_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    return;
  }
  if (decoder_state_ == DecoderState::kAwaitingAssignPictureBuffers) {
    DVLOGF(3) << "Reuse picture buffer requested while waiting for picture "
                 "buffers to be assigned, ignoring";
    return;
  }

  auto it = client_video_frames_.find(picture_buffer_id);
  if (it == client_video_frames_.end()) {
    DVLOGF(3) << picture_buffer_id << " has already been dismissed, ignoring";
    return;
  }

  // Reduce the video frame's use count. If the use count reaches zero we can
  // release our reference to the video frame, returning it to the pool.
  size_t& use_count = it->second.second;
  DCHECK_NE(use_count, 0u);
  if (--use_count == 0) {
    client_video_frames_.erase(it);
  }
}

void GpuArcVideoDecoder::Flush(FlushCallback callback) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Using unretained is safe here, all callbacks are guaranteed to be executed
  // before the decoder is destroyed.
  HandleRequest(base::BindOnce(&GpuArcVideoDecoder::HandleFlushRequest,
                               base::Unretained(this), std::move(callback)));
}

void GpuArcVideoDecoder::Reset(ResetCallback callback) {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Using unretained is safe here, all callbacks are guaranteed to be executed
  // before the decoder is destroyed.
  HandleRequest(base::BindOnce(&GpuArcVideoDecoder::HandleResetRequest,
                               base::Unretained(this), std::move(callback)));
}

void GpuArcVideoDecoder::RequestFrames(
    const media::Fourcc& fourcc,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    size_t max_num_frames,
    NotifyLayoutChangedCb notify_layout_changed_cb,
    ImportFrameCb import_frame_cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!notify_layout_changed_cb_);

  if (decoder_state_ == DecoderState::kError) {
    std::move(notify_layout_changed_cb).Run(absl::nullopt);
    return;
  }

  // Drop all references to currently allocated video frames, which is important
  // as the new imported picture buffers will use the same ids. This is safe as
  // the client manages buffer lifetime and maintains its own references.
  client_video_frames_.clear();

  // Don't apply the new size yet, as the client might still send buffers with
  // the old size until it calls AssignPictureBuffers().
  requested_coded_size_ = coded_size;

  notify_layout_changed_cb_ = std::move(notify_layout_changed_cb);
  import_frame_cb_ = std::move(import_frame_cb);
  decoder_state_ = DecoderState::kAwaitingAssignPictureBuffers;

  auto pbf = mojom::PictureBufferFormat::New();
  pbf->min_num_buffers = max_num_frames;
  pbf->coded_size = coded_size;
  client_->ProvidePictureBuffers(std::move(pbf), visible_rect);
}

void GpuArcVideoDecoder::OnFrameReleased(
    scoped_refptr<media::VideoFrame> origin_frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = frame_id_to_picture_id_.find(
      media::DmabufVideoFramePool::GetDmabufId(*origin_frame));
  DCHECK(it != frame_id_to_picture_id_.end());
  frame_id_to_picture_id_.erase(it);
}

void GpuArcVideoDecoder::OnInitializeDone(media::Status status) {
  DVLOGF(4) << "status: " << status.code();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  HandleInitializeDone(status.is_ok() ? Result::SUCCESS
                                      : Result::PLATFORM_FAILURE);
}

void GpuArcVideoDecoder::OnDecodeDone(int32_t bitstream_buffer_id,
                                      media::Status status) {
  DVLOGF(4) << "status: " << status.code();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!status.is_ok() && (status.code() != media::StatusCode::kAborted)) {
    OnError(FROM_HERE, Result::PLATFORM_FAILURE);
    return;
  }

  client_->NotifyEndOfBitstreamBuffer(bitstream_buffer_id);
}

void GpuArcVideoDecoder::OnFrameReady(scoped_refptr<media::VideoFrame> frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame);

  auto it = frame_id_to_picture_id_.find(
      media::DmabufVideoFramePool::GetDmabufId(*frame.get()));
  if (it == frame_id_to_picture_id_.end()) {
    VLOGF(1) << "Failed to get picture id.";
    OnError(FROM_HERE, Result::PLATFORM_FAILURE);
    return;
  }

  // Add frame to the list of video frames sent to the client. If the video
  // frame is already sent to the client (VP9 show_existing_frame feature),
  // increase its use count.
  int32_t picture_buffer_id = it->second;
  int32_t bitstream_id = FakeTimestampToBitstreamId(frame->timestamp());
  gfx::Rect visible_rect = frame->visible_rect();
  auto frame_it = client_video_frames_.find(picture_buffer_id);
  if (frame_it == client_video_frames_.end()) {
    client_video_frames_.emplace(picture_buffer_id,
                                 std::make_pair(std::move(frame), 1));
  } else {
    frame_it->second.second++;
  }

  mojom::PicturePtr picture = mojom::Picture::New();
  picture->picture_buffer_id = picture_buffer_id;
  picture->bitstream_id = bitstream_id;
  picture->crop_rect = visible_rect;
  client_->PictureReady(std::move(picture));
}

void GpuArcVideoDecoder::OnFlushDone(media::Status status) {
  VLOGF(2) << "status: " << status.code();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (flush_callbacks_.empty()) {
    VLOGF(1) << "Unexpected OnFlushDone() callback received from VD";
    OnError(FROM_HERE, Result::PLATFORM_FAILURE);
    return;
  }

  // Flush requests are processed in the order they were queued.
  switch (status.code()) {
    case media::StatusCode::kOk:
      std::move(flush_callbacks_.front()).Run(Result::SUCCESS);
      flush_callbacks_.pop();
      break;
    case media::StatusCode::kAborted:
      std::move(flush_callbacks_.front()).Run(Result::CANCELLED);
      flush_callbacks_.pop();
      break;
    default:
      OnError(FROM_HERE, Result::PLATFORM_FAILURE);
  }
}

void GpuArcVideoDecoder::OnResetDone() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(flush_callbacks_.empty());

  if (!reset_callback_) {
    VLOGF(1) << "Unexpected OnResetDone() callback received from VD";
    OnError(FROM_HERE, Result::PLATFORM_FAILURE);
    return;
  }

  std::move(reset_callback_).Run(Result::SUCCESS);
  HandleRequests();
}

void GpuArcVideoDecoder::OnError(base::Location location, Result error) {
  VLOGF(1) << "error: " << static_cast<int>(error);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (decoder_state_ == DecoderState::kError) {
    return;
  }

  // Call all in-progress flush and reset callbacks with PLATFORM_FAILURE.
  while (!flush_callbacks_.empty()) {
    std::move(flush_callbacks_.front()).Run(Result::PLATFORM_FAILURE);
    flush_callbacks_.pop();
  }
  if (reset_callback_) {
    std::move(reset_callback_).Run(Result::PLATFORM_FAILURE);
  }

  decoder_state_ = DecoderState::kError;
  if (client_) {
    client_->NotifyError(error);
  }

  // Abort all pending requests.
  HandleRequests();
}

void GpuArcVideoDecoder::HandleInitializeDone(Result result) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  decoder_state_ = (result == Result::SUCCESS) ? DecoderState::kDecoding
                                               : DecoderState::kError;
  // Report initialization status to UMA.
  UMA_HISTOGRAM_ENUMERATION("Media.GpuArcVideoDecoder.InitializeResult",
                            result);
  std::move(init_callback_).Run(result);
}

void GpuArcVideoDecoder::HandleRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!reset_callback_ && !requests_.empty()) {
    HandleRequest(std::move(requests_.front()));
    requests_.pop();
  }
}

void GpuArcVideoDecoder::HandleRequest(Request request) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Postpone all requests if we are currently resetting the decoder. Note that
  // there is no need to postpone requests while flushing. Calling reset while
  // flushing is allowed, and multiple ongoing flush calls are also valid.
  if (reset_callback_) {
    requests_.emplace(std::move(request));
    return;
  }

  std::move(request).Run();
}

void GpuArcVideoDecoder::HandleDecodeRequest(
    int bitstream_id,
    scoped_refptr<media::DecoderBuffer> buffer) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (decoder_state_ == DecoderState::kError) {
    return;
  }
  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    return;
  }

  decoder_->Decode(std::move(buffer),
                   base::BindOnce(&GpuArcVideoDecoder::OnDecodeDone, weak_this_,
                                  bitstream_id));
}

void GpuArcVideoDecoder::HandleFlushRequest(FlushCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  if (decoder_state_ == DecoderState::kError) {
    std::move(callback).Run(Result::PLATFORM_FAILURE);
    return;
  }
  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    std::move(callback).Run(Result::ILLEGAL_STATE);
    return;
  }

  flush_callbacks_.emplace(std::move(callback));
  decoder_->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      base::BindOnce(&GpuArcVideoDecoder::OnFlushDone, weak_this_));
}

void GpuArcVideoDecoder::HandleResetRequest(ResetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOGF(4);

  if (decoder_state_ == DecoderState::kError) {
    std::move(callback).Run(Result::PLATFORM_FAILURE);
    return;
  }
  if (!decoder_) {
    VLOGF(1) << "VD not initialized";
    std::move(callback).Run(Result::ILLEGAL_STATE);
    return;
  }

  reset_callback_ = std::move(std::move(callback));
  decoder_->Reset(base::BindOnce(&GpuArcVideoDecoder::OnResetDone, weak_this_));
}

scoped_refptr<media::DecoderBuffer> GpuArcVideoDecoder::CreateDecoderBuffer(
    base::ScopedFD fd,
    uint32_t offset,
    uint32_t bytes_used) {
  base::subtle::PlatformSharedMemoryRegion shm_region;
  if (*secure_mode_) {
    // Use protected shared memory associated with the given file descriptor.
    shm_region = protected_buffer_manager_->GetProtectedSharedMemoryRegionFor(
        std::move(fd));
    if (!shm_region.IsValid()) {
      VLOGF(1) << "No protected shared memory found for handle";
      return nullptr;
    }
  } else {
    size_t size;
    if (!media::GetFileSize(fd.get(), &size)) {
      VLOGF(1) << "Failed to get size for fd";
      return nullptr;
    }
    shm_region = base::subtle::PlatformSharedMemoryRegion::Take(
        std::move(fd), base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
        size, base::UnguessableToken::Create());
    if (!shm_region.IsValid()) {
      VLOGF(1) << "Cannot take file descriptor based shared memory";
      return nullptr;
    }
  }

  // Create a decoder buffer from the shared memory region.
  return media::DecoderBuffer::FromSharedMemoryRegion(std::move(shm_region),
                                                      offset, bytes_used);
}

gfx::GpuMemoryBufferHandle GpuArcVideoDecoder::CreateGpuMemoryHandle(
    base::ScopedFD fd,
    const std::vector<VideoFramePlane>& planes,
    media::VideoPixelFormat pixel_format,
    uint64_t modifier) {
  gfx::GpuMemoryBufferHandle gmb_handle;
  DCHECK(secure_mode_.has_value());
  if (*secure_mode_) {
    // Get the protected buffer associated with the |fd|.
    gfx::NativePixmapHandle protected_native_pixmap =
        protected_buffer_manager_->GetProtectedNativePixmapHandleFor(
            std::move(fd));
    if (protected_native_pixmap.planes.size() == 0) {
      VLOGF(1) << "No protected native pixmap found for handle";
      return gfx::GpuMemoryBufferHandle();
    }
    gmb_handle.type = gfx::NATIVE_PIXMAP;
    gmb_handle.native_pixmap_handle = std::move(protected_native_pixmap);
  } else {
    std::vector<base::ScopedFD> fds = DuplicateFD(std::move(fd), planes.size());
    if (fds.empty()) {
      VLOGF(1) << "Failed to duplicate fd";
      return gfx::GpuMemoryBufferHandle();
    }
    auto handle = CreateGpuMemoryBufferHandle(
        pixel_format, modifier, coded_size_, std::move(fds), planes);
    if (!handle) {
      VLOGF(1) << "Failed to create GpuMemoryBufferHandle";
      return gfx::GpuMemoryBufferHandle();
    }
    gmb_handle = std::move(handle).value();
  }
  return gmb_handle;
}

scoped_refptr<media::VideoFrame> GpuArcVideoDecoder::CreateVideoFrame(
    gfx::GpuMemoryBufferHandle gmb_handle,
    media::VideoPixelFormat pixel_format) const {
  std::vector<base::ScopedFD> fds;
  for (auto& plane : gmb_handle.native_pixmap_handle.planes) {
    fds.push_back(std::move(plane.fd));
  }
  return media::VideoFrame::WrapExternalDmabufs(
      *video_frame_layout_, gfx::Rect(coded_size_), coded_size_, std::move(fds),
      base::TimeDelta());
}

}  // namespace arc
