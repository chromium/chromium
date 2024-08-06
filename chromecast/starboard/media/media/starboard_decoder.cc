// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard_decoder.h"

#include <utility>

#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/starboard/media/cdm/starboard_drm_key_tracker.h"

namespace chromecast {
namespace media {

using BufferStatus = ::chromecast::media::MediaPipelineBackend::BufferStatus;

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
    DCHECK_EQ(status, BufferStatus::kBufferPending);
  }
}

void StarboardDecoder::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Stopping "
            << (media_type_ == kStarboardMediaTypeAudio ? "audio " : "video ")
            << "decoder";

  pending_first_push_.Reset();
  pending_drm_key_.Reset();
  if (drm_key_token_) {
    StarboardDrmKeyTracker::GetInstance().UnregisterCallback(*drm_key_token_);
    drm_key_token_ = std::nullopt;
  }

  // By setting this to null, we will not push any more buffers until Initialize
  // is called again.
  player_ = nullptr;
}

bool StarboardDecoder::IsInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return player_ != nullptr;
}

void StarboardDecoder::OnBufferWritten() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnPushBufferComplete(BufferStatus::kBufferSuccess);
}

void StarboardDecoder::SetDecoderDelegate(
    MediaPipelineBackend::Decoder::Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = delegate;
}

BufferStatus StarboardDecoder::PushBufferInternal(
    StarboardSampleInfo sample_info,
    DrmInfoWrapper drm_info,
    std::unique_ptr<uint8_t[]> buffer_data,
    size_t buffer_data_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(buffer_data);
  DCHECK_GT(buffer_data_size, 0UL);

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
        std::move(sample_info), std::move(drm_info), std::move(buffer_data),
        buffer_data_size);
    return BufferStatus::kBufferPending;
  }

  DCHECK(buffer_data);
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
      const size_t key_hash = base::FastHash(base::make_span(
          drm_sample_info->identifier,
          static_cast<size_t>(drm_sample_info->identifier_size)));
      LOG(INFO) << "Waiting for DRM key with hash: " << key_hash;
      pending_drm_key_ = base::BindOnce(
          &StarboardDecoder::PushBufferInternal, base::Unretained(this),
          std::move(sample_info), std::move(drm_info), std::move(buffer_data),
          buffer_data_size);

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

  const uint8_t* buffer_addr = buffer_data.get();

  // Ensure that we do not delete the media data until Deallocate is called.
  const bool inserted =
      copied_buffers_.insert({buffer_addr, std::move(buffer_data)}).second;
  DCHECK(inserted) << "Duplicate memory address in copied_buffers_: "
                   << static_cast<const void*>(buffer_addr);

  sample_info.buffer = static_cast<const void*>(buffer_addr);
  sample_info.buffer_size = buffer_data_size;
  sample_info.drm_info = drm_info.GetDrmSampleInfo();

  starboard_->WriteSample(player_, media_type_, &sample_info,
                          /*sample_infos_count=*/1);

  // Returning kBufferPending here means that another buffer will not be pushed
  // until Decoder::Delegate::OnPushBufferComplete is called.
  return BufferStatus::kBufferPending;
}

BufferStatus StarboardDecoder::PushEndOfStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);

  starboard_->WriteEndOfStream(player_, media_type_);
  return BufferStatus::kBufferSuccess;
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
