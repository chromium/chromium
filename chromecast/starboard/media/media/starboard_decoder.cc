// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard_decoder.h"

#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"

namespace chromecast {
namespace media {

using BufferStatus = ::chromecast::media::MediaPipelineBackend::BufferStatus;

StarboardDecoder::InFlightWrite::InFlightWrite(StarboardSampleInfo sample_info,
                                               DrmInfoWrapper drm_info,
                                               const uint8_t* buffer_addr,
                                               size_t buffer_size)
    : sample_info(std::move(sample_info)),
      drm_info(std::move(drm_info)),
      buffer_addr(buffer_addr),
      buffer_size(buffer_size) {}

StarboardDecoder::InFlightWrite::InFlightWrite(InFlightWrite&& other) = default;

StarboardDecoder::InFlightWrite& StarboardDecoder::InFlightWrite::operator=(
    InFlightWrite&& other) = default;

StarboardDecoder::InFlightWrite::~InFlightWrite() = default;

StarboardDecoder::StarboardDecoder(StarboardApiWrapper* starboard,
                                   StarboardMediaType media_type)
    : starboard_(starboard), media_type_(media_type) {
  CHECK(starboard_);
}

StarboardDecoder::~StarboardDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (drm_key_token_) {
    StarboardDrmKeyTracker::GetInstance().UnregisterCallback(*drm_key_token_);
  }
}

void StarboardDecoder::Deallocate(const uint8_t* buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(buffer);

  auto it = copied_buffers_.find(buffer);
  if (it == copied_buffers_.end()) {
    // Since the MediaPipelineBackendStarboard does not know which decoder
    // owns the buffer, this buffer may belong to another decoder. A no-op for
    // this decoder.
    return;
  }

  if (in_flight_write_ && in_flight_write_->buffer_addr == buffer) {
    // Starboard released the sample's data before reporting the write
    // complete. The sample can no longer be re-sent by RequeueInFlightWrite()
    // if the SbPlayer is recreated before OnBufferWritten(); it is dropped.
    in_flight_write_->buffer_addr = nullptr;
  }

  // This frees the memory via the unique_ptr's destructor.
  copied_buffers_.erase(it);
}

void StarboardDecoder::Initialize(void* sb_player) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sb_player);

  player_ = sb_player;
  InitializeInternal();

  if (pending_first_push_) {
    LOG(INFO) << "Pushing pending "
              << (media_type_ == kStarboardMediaTypeAudio ? "audio " : "video ")
              << "buffer";
    const BufferStatus status = std::move(pending_first_push_).Run();
    CHECK_NE(status, BufferStatus::kBufferFailed);

    if (status == BufferStatus::kBufferSuccess) {
      // We pushed EoS.
      in_flight_write_.reset();
      delegate_->OnPushBufferComplete(BufferStatus::kBufferSuccess);
    }
    // For the kBufferPending case, we wait until
    // StarboardDecoder::OnBufferWritten is called to inform the delegate that
    // the push is complete.
  }
}

void StarboardDecoder::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Stopping "
            << (media_type_ == kStarboardMediaTypeAudio ? "audio " : "video ")
            << "decoder";

  pending_first_push_.Reset();
  pending_drm_key_.Reset();
  in_flight_write_.reset();
  if (drm_key_token_) {
    StarboardDrmKeyTracker::GetInstance().UnregisterCallback(*drm_key_token_);
    drm_key_token_ = std::nullopt;
  }

  // By setting this to null, we will not push any more buffers until Initialize
  // is called again.
  player_ = nullptr;
  StopInternal();
}

void StarboardDecoder::ResetPlayerForRecreation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  player_ = nullptr;
}

bool StarboardDecoder::IsInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return player_ != nullptr;
}

void StarboardDecoder::OnBufferWritten() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  // `pending_drm_key_` is intentionally left alone: it holds a buffer parked by
  // PushBufferInternal() until its DRM key arrives and is run or dropped
  // together with `drm_key_token_`.
  in_flight_write_.reset();
  delegate_->OnPushBufferComplete(BufferStatus::kBufferSuccess);
}

void StarboardDecoder::RequeueInFlightWrite() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (drm_key_token_) {
    StarboardDrmKeyTracker::GetInstance().UnregisterCallback(*drm_key_token_);
    drm_key_token_ = std::nullopt;
  }
  if (pending_drm_key_) {
    pending_first_push_ = std::move(pending_drm_key_);
    return;
  }
  if (!in_flight_write_) {
    return;
  }
  InFlightWrite in_flight = std::move(*in_flight_write_);
  in_flight_write_.reset();
  auto it = copied_buffers_.find(in_flight.buffer_addr);
  if (it == copied_buffers_.end()) {
    return;
  }
  base::HeapArray<uint8_t> requeue_copy =
      base::HeapArray<uint8_t>::CopiedFrom(it->second.as_span());
  pending_first_push_ =
      base::BindOnce(&StarboardDecoder::PushBufferInternal,
                     base::Unretained(this), std::move(in_flight.sample_info),
                     std::move(in_flight.drm_info), std::move(requeue_copy));
}

void StarboardDecoder::SetDecoderDelegate(
    MediaPipelineBackend::Decoder::Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = delegate;
}

BufferStatus StarboardDecoder::PushBufferInternal(
    StarboardSampleInfo sample_info,
    DrmInfoWrapper drm_info,
    base::HeapArray<uint8_t> buffer_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!buffer_data.empty());

  if (!player_) {
    if (pending_first_push_) {
      LOG(WARNING) << "PushBuffer was called multiple times for "
                   << (media_type_ == kStarboardMediaTypeAudio ? "audio "
                                                               : "video ")
                   << "buffers before the decoder was initialized. Dropping "
                      "the old buffer.";
    } else {
      LOG(INFO) << "StarboardDecoder was not initialized before first "
                   "PushBuffer. Delaying push until initialization.";
    }

    // Use of base::Unretained is safe here because pending_first_push_ will
    // only be called by this object (implying that `this` will not have been
    // destructed).
    pending_first_push_ = base::BindOnce(
        &StarboardDecoder::PushBufferInternal, base::Unretained(this),
        std::move(sample_info), std::move(drm_info), std::move(buffer_data));
    return BufferStatus::kBufferPending;
  }

  DCHECK(delegate_);
  DCHECK(!pending_first_push_);

  // For encrypted buffers, we should not push data to starboard util the
  // buffer's DRM key is available to the CDM. To accomplish this, we check with
  // the StarboardDrmKeyTracker singleton -- which is updated by the CDM,
  // StarboardDecryptorCast -- to see whether the key is available. If the key
  // is not available yet, we register a callback that will be run once the key
  // becomes available.
  if (StarboardDrmSampleInfo* drm_sample_info = drm_info.GetDrmSampleInfo();
      drm_sample_info != nullptr) {
    const std::string drm_key(
        reinterpret_cast<const char*>(&drm_sample_info->identifier),
        drm_sample_info->identifier_size);
    if (!StarboardDrmKeyTracker::GetInstance().HasKey(drm_key)) {
      // They key is not available yet; register a callback to push the buffer
      // once the key becomes available.
      CHECK_GE(drm_sample_info->identifier_size, 0);
      const size_t key_hash = base::FastHash(
          base::span(drm_sample_info->identifier)
              .first(static_cast<size_t>(drm_sample_info->identifier_size)));
      LOG(INFO) << "Waiting for DRM key with hash: " << key_hash;
      pending_drm_key_ = base::BindOnce(
          &StarboardDecoder::PushBufferInternal, base::Unretained(this),
          std::move(sample_info), std::move(drm_info), std::move(buffer_data));

      CHECK(base::SequencedTaskRunner::HasCurrentDefault());
      drm_key_token_ = StarboardDrmKeyTracker::GetInstance().WaitForKey(
          drm_key,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindOnce(&StarboardDecoder::RunPendingDrmKeyCallback,
                             weak_factory_.GetWeakPtr())));
      return BufferStatus::kBufferPending;
    }

    // The key is already available; continue the logic of pushing the buffer to
    // starboard.
  }

  const uint8_t* buffer_addr = buffer_data.data();
  const size_t buffer_size = buffer_data.size();
  sample_info.buffer = static_cast<const void*>(buffer_addr);
  sample_info.buffer_size = base::checked_cast<int>(buffer_size);
  sample_info.drm_info = drm_info.GetDrmSampleInfo();

  // Ensure that we do not delete the media data until Deallocate is called.
  const bool inserted =
      copied_buffers_.insert({buffer_addr, std::move(buffer_data)}).second;
  DCHECK(inserted) << "Duplicate memory address in copied_buffers_: "
                   << static_cast<const void*>(buffer_addr);

  in_flight_write_.emplace(sample_info, std::move(drm_info), buffer_addr,
                           buffer_size);
  starboard_->WriteSample(player_, media_type_,
                          base::span_from_ref(sample_info));

  // Returning kBufferPending here means that another buffer will not be pushed
  // until Decoder::Delegate::OnPushBufferComplete is called.
  return BufferStatus::kBufferPending;
}

BufferStatus StarboardDecoder::PushEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);

  if (player_) {
    starboard_->WriteEndOfStream(player_, media_type_);
    return BufferStatus::kBufferSuccess;
  }

  // The decoder is not yet initialized, e.g. because an EoS buffer is the first
  // push after a seek.
  if (pending_first_push_) {
    LOG(WARNING) << "PushBuffer was called multiple times for "
                 << (media_type_ == kStarboardMediaTypeAudio ? "audio "
                                                             : "video ")
                 << "buffers before the decoder was initialized. Dropping "
                    "the old buffer.";
  } else {
    LOG(INFO) << "StarboardDecoder was not initialized before first "
                 "PushBuffer. Delaying push until initialization.";
  }

  // Use of base::Unretained is safe here because pending_first_push_ will
  // only be called by this object (implying that `this` will not have been
  // destructed).
  pending_first_push_ = base::BindOnce(&StarboardDecoder::PushEndOfStream,
                                       base::Unretained(this));
  return BufferStatus::kBufferPending;
}

void* StarboardDecoder::GetPlayer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return player_;
}

StarboardApiWrapper& StarboardDecoder::GetStarboardApi() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *starboard_;
}

MediaPipelineBackend::Decoder::Delegate* StarboardDecoder::GetDelegate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_;
}

void StarboardDecoder::OnSbPlayerEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_) {
    delegate_->OnEndOfStream();
  }
}

void StarboardDecoder::OnStarboardDecodeError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_) {
    delegate_->OnDecoderError();
  }
}

void StarboardDecoder::RunPendingDrmKeyCallback(int64_t token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!drm_key_token_) {
    LOG(INFO)
        << "Pending DRM key callback was run after drm_key_token_ was cleared.";
    return;
  }

  if (*drm_key_token_ != token) {
    LOG(INFO) << "Pending DRM key callback was called for a token that does "
                 "not match the expected token. Expected token: "
              << *drm_key_token_ << ", received token: " << token;
    return;
  }

  // Clear the token, since we are no longer waiting to run the callback.
  drm_key_token_ = std::nullopt;

  LOG(INFO) << "Running DRM key callback";
  CHECK_EQ(std::move(pending_drm_key_).Run(), BufferStatus::kBufferPending);
}

}  // namespace media
}  // namespace chromecast
