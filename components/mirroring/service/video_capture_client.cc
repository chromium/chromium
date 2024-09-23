// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/video_capture_client.h"

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"

namespace mirroring {

namespace {

// Required by mojom::VideoCaptureHost interface. Can be any nonzero value.
const base::UnguessableToken& DeviceId() {
  // TODO(crbug.com/40252974): Investigate whether there's a better way
  // to accomplish this (without using UnguessableToken::Deserialize).
  static const base::UnguessableToken device_id(
      base::UnguessableToken::Deserialize(1, 1).value());
  return device_id;
}

// Required by mojom::VideoCaptureHost interface. Can be any nonzero value.
const base::UnguessableToken& SessionId() {
  // TODO(crbug.com/40252974): Investigate whether there's a better way
  // to accomplish this (without using UnguessableToken::Deserialize).
  static const base::UnguessableToken session_id(
      base::UnguessableToken::Deserialize(1, 1).value());
  return session_id;
}

}  // namespace

VideoCaptureClient::VideoCaptureClient(
    const media::VideoCaptureParams& params,
    mojo::PendingRemote<media::mojom::VideoCaptureHost> host)
    : params_(params), video_capture_host_(std::move(host)) {
  DCHECK(video_capture_host_);
}

VideoCaptureClient::~VideoCaptureClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

void VideoCaptureClient::Start(FrameDeliverCallback deliver_callback,
                               base::OnceClosure error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << __func__;
  DCHECK(!deliver_callback.is_null());
  frame_deliver_callback_ = std::move(deliver_callback);
  error_callback_ = std::move(error_callback);

  video_capture_host_->Start(DeviceId(), SessionId(), params_,
                             receiver_.BindNewPipeAndPassRemote());
}

void VideoCaptureClient::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << __func__;
  video_capture_host_->Stop(DeviceId());
}

void VideoCaptureClient::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__;
  if (frame_deliver_callback_.is_null())
    return;
  frame_deliver_callback_.Reset();
  video_capture_host_->Pause(DeviceId());
}

void VideoCaptureClient::Resume(FrameDeliverCallback deliver_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__;
  DCHECK(!deliver_callback.is_null());
  if (!frame_deliver_callback_.is_null()) {
    return;
  }
  frame_deliver_callback_ = std::move(deliver_callback);
  video_capture_host_->Resume(DeviceId(), SessionId(), params_);
}

void VideoCaptureClient::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (frame_deliver_callback_.is_null())
    return;
  video_capture_host_->RequestRefreshFrame(DeviceId());
}

void VideoCaptureClient::SwitchVideoCaptureHost(
    mojo::PendingRemote<media::mojom::VideoCaptureHost> host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__;
  switching_video_capture_host_ = true;
  video_capture_host_.reset();
  video_capture_host_.Bind(std::move(host));
  DCHECK(video_capture_host_);
}

void VideoCaptureClient::OnStateChanged(
    media::mojom::VideoCaptureResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result->which() == media::mojom::VideoCaptureResult::Tag::kState) {
    media::mojom::VideoCaptureState state = result->get_state();
    DVLOG(2) << __func__ << " state: " << state;
    switch (state) {
      case media::mojom::VideoCaptureState::STARTED:
        RequestRefreshFrame();
        break;
      case media::mojom::VideoCaptureState::PAUSED:
      case media::mojom::VideoCaptureState::RESUMED:
        break;
      case media::mojom::VideoCaptureState::STOPPED:
      case media::mojom::VideoCaptureState::ENDED:
        client_buffers_.clear();
        weak_factory_.InvalidateWeakPtrs();
        receiver_.reset();
        if (switching_video_capture_host_) {
          switching_video_capture_host_ = false;
          first_frame_ref_time_ = base::TimeTicks();
          accumulated_time_ = last_timestamp_;
          Start(std::move(frame_deliver_callback_), std::move(error_callback_));
        } else {
          error_callback_.Reset();
          frame_deliver_callback_.Reset();
        }
        break;
    }
  } else {
    DVLOG(2) << __func__ << " Failed with an error.";
    if (!error_callback_.is_null())
      std::move(error_callback_).Run();
  }
}

void VideoCaptureClient::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << ": buffer_id=" << buffer_id;

  if (!buffer_handle->is_read_only_shmem_region() &&
      !buffer_handle->is_unsafe_shmem_region()) {
#if BUILDFLAG(IS_MAC)
    if (!buffer_handle->is_gpu_memory_buffer_handle()) {
      NOTIMPLEMENTED();
      return;
    }
#else
    NOTIMPLEMENTED();
    return;
#endif
  }
  const auto insert_result =
      client_buffers_.insert({buffer_id, std::move(buffer_handle)});
  DCHECK(insert_result.second);
}

void VideoCaptureClient::OnBufferReady(media::mojom::ReadyBufferPtr buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << ": buffer_id=" << buffer->buffer_id;

  bool consume_buffer = !frame_deliver_callback_.is_null();
  if (buffer->info->pixel_format != media::PIXEL_FORMAT_NV12 &&
      buffer->info->pixel_format != media::PIXEL_FORMAT_I420 &&
      buffer->info->pixel_format != media::PIXEL_FORMAT_Y16) {
    consume_buffer = false;
    LOG(DFATAL) << "Wrong pixel format, got pixel format:"
                << VideoPixelFormatToString(buffer->info->pixel_format);
  }
  if (!consume_buffer) {
    video_capture_host_->ReleaseBuffer(DeviceId(), buffer->buffer_id,
                                       media::VideoCaptureFeedback());
    return;
  }

  base::TimeTicks reference_time = *buffer->info->metadata.reference_time;

  if (first_frame_ref_time_.is_null())
    first_frame_ref_time_ = reference_time;

  // If the timestamp is not prepared, we use reference time to make a rough
  // estimate. e.g. ThreadSafeCaptureOracle::DidCaptureFrame().
  // TODO(crbug.com/40472286): Fix upstream capturers to always set timestamp
  // and reference time.
  if (buffer->info->timestamp.is_zero())
    buffer->info->timestamp = reference_time - first_frame_ref_time_;

  // Used by chrome/browser/media/cast_mirroring_performance_browsertest.cc
  TRACE_EVENT_INSTANT2("cast_perf_test", "OnBufferReceived",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       (reference_time - base::TimeTicks()).InMicroseconds(),
                       "time_delta", buffer->info->timestamp.InMicroseconds());

  const auto& buffer_iter = client_buffers_.find(buffer->buffer_id);
  if (buffer_iter == client_buffers_.end()) {
    LOG(DFATAL) << "Ignoring OnBufferReady() for unknown buffer.";
    return;
  }
  scoped_refptr<media::VideoFrame> frame;
  BufferFinishedCallback buffer_finished_callback;
  if (buffer_iter->second->is_gpu_memory_buffer_handle()) {
#if BUILDFLAG(IS_MAC)
    frame = media::VideoFrame::WrapUnacceleratedIOSurface(
        buffer_iter->second->get_gpu_memory_buffer_handle().Clone(),
        buffer->info->visible_rect, buffer->info->timestamp);
    buffer_finished_callback =
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &VideoCaptureClient::OnClientBufferFinished,
            weak_factory_.GetWeakPtr(), buffer->buffer_id, MappingKeepAlive()));
#else
    NOTREACHED_IN_MIGRATION();
#endif
  } else if (buffer_iter->second->is_unsafe_shmem_region()) {
    base::WritableSharedMemoryMapping mapping =
        buffer_iter->second->get_unsafe_shmem_region().Map();
    const size_t frame_allocation_size = media::VideoFrame::AllocationSize(
        buffer->info->pixel_format, buffer->info->coded_size);
    if (mapping.IsValid() && mapping.size() >= frame_allocation_size) {
      frame = media::VideoFrame::WrapExternalData(
          buffer->info->pixel_format, buffer->info->coded_size,
          buffer->info->visible_rect, buffer->info->visible_rect.size(),
          mapping.GetMemoryAs<uint8_t>(), frame_allocation_size,
          buffer->info->timestamp);
    }
    buffer_finished_callback =
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &VideoCaptureClient::OnClientBufferFinished,
            weak_factory_.GetWeakPtr(), buffer->buffer_id, std::move(mapping)));
  } else {
    // Duplicate base::ReadOnlySharedMemoryRegion here because there is no
    // guarantee on lifetime between |client_buffers_| and |frame|.
    base::ReadOnlySharedMemoryRegion shm_region =
        buffer_iter->second->get_read_only_shmem_region().Duplicate();
    base::ReadOnlySharedMemoryMapping mapping = shm_region.Map();
    const size_t frame_allocation_size = media::VideoFrame::AllocationSize(
        buffer->info->pixel_format, buffer->info->coded_size);
    if (mapping.IsValid() && mapping.size() >= frame_allocation_size) {
      frame = media::VideoFrame::WrapExternalData(
          buffer->info->pixel_format, buffer->info->coded_size,
          buffer->info->visible_rect, buffer->info->visible_rect.size(),
          mapping.GetMemoryAs<uint8_t>(), frame_allocation_size,
          buffer->info->timestamp);
      if (frame) {
        frame->BackWithOwnedSharedMemory(std::move(shm_region),
                                         std::move(mapping));
      }
    }
    buffer_finished_callback = base::BindPostTaskToCurrentDefault(
        base::BindOnce(&VideoCaptureClient::OnClientBufferFinished,
                       weak_factory_.GetWeakPtr(), buffer->buffer_id,
                       base::ReadOnlySharedMemoryMapping()));
  }

  if (!frame) {
    LOG(DFATAL) << "Unable to wrap shared memory mapping.";
    video_capture_host_->ReleaseBuffer(DeviceId(), buffer->buffer_id,
                                       media::VideoCaptureFeedback());

    OnStateChanged(media::mojom::VideoCaptureResult::NewErrorCode(
        media::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16));
    return;
  }
  frame->AddDestructionObserver(
      base::BindOnce(&VideoCaptureClient::DidFinishConsumingFrame,
                     std::move(buffer_finished_callback)));

  // Convert NV12 frames to I420, because NV12 is not supported by Cast
  // Streaming.
  // https://crbug.com/1206325
  if (frame->format() == media::PIXEL_FORMAT_NV12) {
    if (!nv12_to_i420_pool_)
      nv12_to_i420_pool_ = std::make_unique<media::VideoFramePool>();
    scoped_refptr<media::VideoFrame> new_frame =
        nv12_to_i420_pool_->CreateFrame(
            media::PIXEL_FORMAT_I420, frame->coded_size(),
            frame->visible_rect(), frame->natural_size(), frame->timestamp());
    media::EncoderStatus status =
        frame_converter_.ConvertAndScale(*frame, *new_frame);
    if (!status.is_ok()) {
      LOG(DFATAL) << "Unable to convert frame to I420.";
      OnStateChanged(media::mojom::VideoCaptureResult::NewErrorCode(
          media::VideoCaptureError::kDeviceClientTooManyFramesDroppedY16));
      return;
    }
    frame = new_frame;
  }

  frame->set_metadata(buffer->info->metadata);
  frame->set_color_space(buffer->info->color_space);

  frame->set_timestamp(frame->timestamp() + accumulated_time_);
  last_timestamp_ = frame->timestamp();

  frame_deliver_callback_.Run(frame);
}

void VideoCaptureClient::OnBufferDestroyed(int32_t buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << ": buffer_id=" << buffer_id;

  const auto& buffer_iter = client_buffers_.find(buffer_id);
  if (buffer_iter != client_buffers_.end())
    client_buffers_.erase(buffer_iter);
}

void VideoCaptureClient::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {}

void VideoCaptureClient::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {}

void VideoCaptureClient::OnClientBufferFinished(int buffer_id,
                                                MappingKeepAlive mapping) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << ": buffer_id=" << buffer_id;

  // Buffer was already destroyed.
  if (client_buffers_.find(buffer_id) == client_buffers_.end()) {
    return;
  }

  video_capture_host_->ReleaseBuffer(DeviceId(), buffer_id, feedback_);
  feedback_ = media::VideoCaptureFeedback();
}

// static
void VideoCaptureClient::DidFinishConsumingFrame(
    BufferFinishedCallback callback) {
  // Note: This function may be called on any thread by the VideoFrame
  // destructor.
  DCHECK(!callback.is_null());
  std::move(callback).Run();
}

void VideoCaptureClient::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  feedback_ = feedback;
}

}  // namespace mirroring
