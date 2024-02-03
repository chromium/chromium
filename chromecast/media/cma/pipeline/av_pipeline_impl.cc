// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/av_pipeline_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/base/decrypt_context_impl.h"
#include "chromecast/media/cdm/cast_cdm_context.h"
#include "chromecast/media/cma/base/buffering_frame_provider.h"
#include "chromecast/media/cma/base/buffering_state.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/pipeline/cdm_decryptor.h"
#include "chromecast/media/cma/pipeline/decrypt_util.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decrypt_config.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

AvPipelineImpl::AvPipelineImpl(CmaBackend::Decoder* decoder,
                               AvPipelineClient client)
    : bytes_decoded_since_last_update_(0),
      decoder_(decoder),
      client_(std::move(client)),
      state_(kUninitialized),
      buffered_time_(::media::kNoTimestamp),
      playable_buffered_time_(::media::kNoTimestamp),
      enable_feeding_(false),
      pending_read_(false),
      cast_cdm_context_(nullptr),
      weak_factory_(this),
      decrypt_weak_factory_(this) {
  DCHECK(decoder_);
  decoder_->SetDelegate(this);
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

AvPipelineImpl::~AvPipelineImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void AvPipelineImpl::SetCodedFrameProvider(
    std::unique_ptr<CodedFrameProvider> frame_provider,
    size_t max_buffer_size,
    size_t max_frame_size) {
  DCHECK_EQ(state_, kUninitialized);
  DCHECK(frame_provider);

  // Wrap the incoming frame provider to add some buffering capabilities.
  frame_provider_.reset(new BufferingFrameProvider(
      std::move(frame_provider), max_buffer_size, max_frame_size,
      base::BindRepeating(&AvPipelineImpl::OnDataBuffered, weak_this_)));
}

bool AvPipelineImpl::StartPlayingFrom(
    base::TimeDelta time,
    const scoped_refptr<BufferingState>& buffering_state) {
  LOG(INFO) << __FUNCTION__ << " t0=" << time.InMilliseconds();
  DCHECK(thread_checker_.CalledOnValidThread());

  // Reset the pipeline statistics.
  previous_stats_ = ::media::PipelineStatistics();

  if (state_ == kError) {
    LOG(INFO) << __FUNCTION__ << " called while in error state";
    return false;
  }
  DCHECK_EQ(state_, kFlushed);

  // Buffering related initialization.
  DCHECK(frame_provider_);
  buffering_state_ = buffering_state;
  if (buffering_state_.get())
    buffering_state_->SetMediaTime(time);

  // Discard any previously pushed buffer and start feeding the pipeline.
  pushed_buffer_ = nullptr;
  enable_feeding_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AvPipelineImpl::FetchBuffer, weak_this_));

  set_state(kPlaying);
  return true;
}

void AvPipelineImpl::Flush(base::OnceClosure flush_cb) {
  LOG(INFO) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(flush_cb_.is_null());

  if (state_ == kError) {
    LOG(INFO) << __FUNCTION__ << " called while in error state";
    return;
  }
  DCHECK_EQ(state_, kPlaying);
  set_state(kFlushing);

  flush_cb_ = std::move(flush_cb);
  // Stop feeding the pipeline.
  // Do not invalidate |pushed_buffer_| here since the backend may still be
  // using it. Invalidate it in StartPlayingFrom on the assumption that
  // the backend will be stopped after this function returns.
  enable_feeding_ = false;
  // Remove any pending buffer.
  pending_buffer_ = nullptr;
  // Remove any frames left in the frame provider.
  pending_read_ = false;
  buffered_time_ = ::media::kNoTimestamp;
  playable_buffered_time_ = ::media::kNoTimestamp;
  non_playable_frames_.clear();

  // Drop any pending asynchronous decryption, so any pending
  // OnBufferDecrypted() callback will not be called. StartPlayingFrom() sets
  // enable_feeding_ back to true, so if a pending decryption callback from
  // before Stop() is allowed to complete after StartPlayingFrom() is called
  // again, it will think everything is fine and try to push a buffer, resulting
  // in a double push.
  decrypt_weak_factory_.InvalidateWeakPtrs();

  ready_buffers_ = {};

  // Reset |decryptor_| to flush buffered frames in |decryptor_|.
  decryptor_.reset();

  frame_provider_->Flush(
      base::BindOnce(&AvPipelineImpl::OnFlushDone, weak_this_));
}

void AvPipelineImpl::OnFlushDone() {
  LOG(INFO) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == kError) {
    // Flush callback is reset on error.
    DCHECK(flush_cb_.is_null());
    return;
  }
  DCHECK_EQ(state_, kFlushing);
  set_state(kFlushed);
  std::move(flush_cb_).Run();
}

void AvPipelineImpl::SetCdm(CastCdmContext* cast_cdm_context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(cast_cdm_context);

  cast_cdm_context_ = cast_cdm_context;
  event_cb_registration_ = cast_cdm_context_->RegisterEventCB(
      base::BindRepeating(&AvPipelineImpl::OnCdmStateChanged, weak_this_));

  // We could be waiting for CDM to provide key (see b/29564232).
  OnCdmStateChanged(::media::CdmContext::Event::kHasAdditionalUsableKey);
}

void AvPipelineImpl::FetchBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enable_feeding_)
    return;

  DCHECK(!pending_read_ && !pending_buffer_);

  pending_read_ = true;
  frame_provider_->Read(
      base::BindOnce(&AvPipelineImpl::OnNewFrame, weak_this_));
}

void AvPipelineImpl::OnNewFrame(
    const scoped_refptr<DecoderBufferBase>& buffer,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_read_ = false;

  if (!enable_feeding_)
    return;

  if (audio_config.IsValidConfig() || video_config.IsValidConfig())
    OnUpdateConfig(buffer->stream_id(), audio_config, video_config);

  pending_buffer_ = buffer;
  ProcessPendingBuffer();
}

void AvPipelineImpl::ProcessPendingBuffer() {
  if (!enable_feeding_)
    return;

  DCHECK(!pushed_buffer_);

  // Break the feeding loop when the end of stream is reached.
  if (pending_buffer_->end_of_stream()) {
    LOG(INFO) << __FUNCTION__ << ": EOS reached, stopped feeding";
    enable_feeding_ = false;
  }

  if (!pending_buffer_->end_of_stream() &&
      pending_buffer_->decrypt_config()) {
    // Verify that CDM has the key ID.
    // Should not send the frame if the key ID is not available yet.
    std::string key_id(pending_buffer_->decrypt_config()->key_id());
    if (!cast_cdm_context_) {
      LOG(INFO) << "No CDM for frame: pts=" << pending_buffer_->timestamp();
      return;
    }

    std::unique_ptr<DecryptContextImpl> decrypt_context =
        cast_cdm_context_->GetDecryptContext(
            key_id, GetEncryptionScheme(pending_buffer_->stream_id()));
    if (!decrypt_context) {
      LOG(INFO) << "frame(pts=" << pending_buffer_->timestamp()
                << "): waiting for key id " << base::HexEncode(key_id);
      if (!client_.waiting_cb.is_null())
        client_.waiting_cb.Run(::media::WaitingReason::kNoDecryptionKey);
      return;
    }

    DCHECK_NE(decrypt_context->GetKeySystem(), KEY_SYSTEM_NONE);

    if (!decryptor_) {
      decryptor_ = CreateStreamDecryptor(decrypt_context->GetKeySystem());
      DCHECK(decryptor_);
      decryptor_->Init(base::BindRepeating(&AvPipelineImpl::OnBufferDecrypted,
                                           decrypt_weak_factory_.GetWeakPtr()));
    }

    pending_buffer_->set_decrypt_context(std::move(decrypt_context));
  }

  if (decryptor_) {
    decryptor_->Decrypt(std::move(pending_buffer_));
    return;
  }

  DCHECK(ready_buffers_.empty());
  PushReadyBuffer(std::move(pending_buffer_));
}

void AvPipelineImpl::PushAllReadyBuffers() {
  if (state_ != kPlaying)
    return;

  DCHECK(!ready_buffers_.empty());

  scoped_refptr<DecoderBufferBase> ready_buffer =
      std::move(ready_buffers_.front());
  ready_buffers_.pop();

  PushReadyBuffer(std::move(ready_buffer));
}

void AvPipelineImpl::PushReadyBuffer(scoped_refptr<DecoderBufferBase> buffer) {
  DCHECK(!pushed_buffer_);

  if (!buffer->end_of_stream() && buffering_state_.get()) {
    base::TimeDelta timestamp = base::Microseconds(buffer->timestamp());
    if (timestamp != ::media::kNoTimestamp)
      buffering_state_->SetMaxRenderingTime(timestamp);
  }

  pushed_buffer_ = std::move(buffer);

  CmaBackend::BufferStatus status = decoder_->PushBuffer(pushed_buffer_);

  if (status != CmaBackend::BufferStatus::kBufferPending)
    OnPushBufferComplete(status);
}

void AvPipelineImpl::OnBufferDecrypted(bool success,
                                       StreamDecryptor::BufferQueue buffers) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!success) {
    OnDecoderError();
    return;
  }

  // Decryptor needs more data.
  if (buffers.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AvPipelineImpl::FetchBuffer, weak_this_));
    return;
  }

  ready_buffers_ = std::move(buffers);
  PushAllReadyBuffers();
}

void AvPipelineImpl::OnPushBufferComplete(BufferStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());

  pushed_buffer_ = nullptr;
  if (status == CmaBackend::BufferStatus::kBufferFailed) {
    LOG(WARNING) << "AvPipelineImpl: PushFrame failed";
    OnDecoderError();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      ready_buffers_.empty()
          ? base::BindOnce(&AvPipelineImpl::FetchBuffer, weak_this_)
          : base::BindOnce(&AvPipelineImpl::PushAllReadyBuffers, weak_this_));
}

void AvPipelineImpl::OnEndOfStream() {
  if (!client_.eos_cb.is_null())
    client_.eos_cb.Run();
}

void AvPipelineImpl::OnDecoderError() {
  enable_feeding_ = false;
  state_ = kError;

  if (!client_.playback_error_cb.is_null())
    client_.playback_error_cb.Run(::media::PIPELINE_ERROR_COULD_NOT_RENDER);

  if (!flush_cb_.is_null())
    std::move(flush_cb_).Run();
}

void AvPipelineImpl::OnKeyStatusChanged(const std::string& key_id,
                                        CastKeyStatus key_status,
                                        uint32_t system_code) {
  LOG(INFO) << __FUNCTION__ << " key_status= " << key_status
            << " system_code=" << system_code;
  DCHECK(cast_cdm_context_);
  cast_cdm_context_->SetKeyStatus(key_id, key_status, system_code);
}

void AvPipelineImpl::OnVideoResolutionChanged(const Size& size) {
  // Ignored here; VideoPipelineImpl overrides this method.
}

void AvPipelineImpl::OnCdmStateChanged(::media::CdmContext::Event event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (event != ::media::CdmContext::Event::kHasAdditionalUsableKey)
    return;

  // Update the buffering state if needed.
  if (buffering_state_.get())
    UpdatePlayableFrames();

  // Process the pending buffer in case the CDM now has the frame key id.
  if (pending_buffer_)
    ProcessPendingBuffer();
}

void AvPipelineImpl::OnDataBuffered(
    const scoped_refptr<DecoderBufferBase>& buffer,
    bool is_at_max_capacity) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!buffering_state_.get())
    return;

  if (!buffer->end_of_stream() &&
      (buffered_time_ == ::media::kNoTimestamp ||
       buffered_time_ < base::Microseconds(buffer->timestamp()))) {
    buffered_time_ = base::Microseconds(buffer->timestamp());
  }

  if (is_at_max_capacity)
    buffering_state_->NotifyMaxCapacity(buffered_time_);

  // No need to update the list of playable frames,
  // if we are already blocking on a frame.
  bool update_playable_frames = non_playable_frames_.empty();
  non_playable_frames_.push_back(buffer);
  if (update_playable_frames)
    UpdatePlayableFrames();
}

void AvPipelineImpl::UpdatePlayableFrames() {
  while (!non_playable_frames_.empty()) {
    const scoped_refptr<DecoderBufferBase>& non_playable_frame =
        non_playable_frames_.front();

    if (non_playable_frame->end_of_stream()) {
      buffering_state_->NotifyEos();
    } else {
      const CastDecryptConfig* decrypt_config =
          non_playable_frame->decrypt_config();
      if (decrypt_config &&
          !(cast_cdm_context_ &&
            cast_cdm_context_
                ->GetDecryptContext(
                    decrypt_config->key_id(),
                    GetEncryptionScheme(non_playable_frame->stream_id()))
                .get())) {
        // The frame is still not playable. All the following are thus not
        // playable.
        break;
      }

      if (playable_buffered_time_ == ::media::kNoTimestamp ||
          playable_buffered_time_ <
              base::Microseconds(non_playable_frame->timestamp())) {
        playable_buffered_time_ =
            base::Microseconds(non_playable_frame->timestamp());
        buffering_state_->SetBufferedTime(playable_buffered_time_);
      }
    }

    // The frame is playable: remove it from the list of non playable frames.
    non_playable_frames_.pop_front();
  }
}

std::unique_ptr<StreamDecryptor> AvPipelineImpl::CreateStreamDecryptor(
    CastKeySystem key_system) {
  if (key_system == KEY_SYSTEM_CLEAR_KEY) {
    // Clear Key only supports clear output.
    return std::make_unique<CdmDecryptor>(true /* clear_buffer_needed */);
  }

  return CreateDecryptor();
}

}  // namespace media
}  // namespace chromecast
