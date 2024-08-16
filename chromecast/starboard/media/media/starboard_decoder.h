// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_DECODER_H_
#define CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_DECODER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "chromecast/starboard/media/media/drm_util.h"
#include "chromecast/starboard/media/media/starboard_api_wrapper.h"

namespace chromecast {
namespace media {

// A base class for StarboardAudioDecoder and StarboardVideoDecoder, containing
// logic common to all decoders. It manages interactions with Starboard and the
// lifetime of buffers.
//
// This is an abstract class; child classes can implement InitializeInternal to
// perform any necessary initialization logic.
//
// All functions, including the constructor and destructor, must be called on
// the same sequence (the media thread).
class StarboardDecoder {
 public:
  // Disallow copy and assign.
  StarboardDecoder(const StarboardDecoder&) = delete;
  StarboardDecoder& operator=(const StarboardDecoder&) = delete;

  // Called when a buffer can be deallocated.
  void Deallocate(const uint8_t* buffer);

  // Initializes the decoder by providing the opaque SbPlayer that will be used
  // to decode/render buffers.  Before this call, no buffers will be pushed to
  // Starboard. Calling Pushbuffer will just queue the buffer.
  // `sb_player` must not be null.
  void Initialize(void* sb_player);

  // Clears any pending buffer, and sets the decoder into an un-initialized
  // state.
  void Stop();

  // Returns true if the decoder is initialized, false otherwise. After Stop(),
  // the decoder is no longer initialized.
  bool IsInitialized();

  // Called when a buffer has been processed by Starboard.
  void OnBufferWritten();

  // Notifies the delegate that the end of stream buffer has been rendered. This
  // should be called once the SbPlayer has sent the
  // kStarboardPlayerStateEndOfStream/kSbPlayerStateEndOfStream signal.
  void OnSbPlayerEndOfStream();

  // Called when the SbPlayer has encountered a decoder error. Notifies the
  // delegate that an error has occurred.
  void OnStarboardDecodeError();

 protected:
  // Calls to Starboard are made via `starboard`, to allow mocking in tests.
  // `media_type` specifies the type of decoder that this represents (audio or
  // video).
  StarboardDecoder(StarboardApiWrapper* starboard,
                   StarboardMediaType media_type);

  virtual ~StarboardDecoder();

  // Sets the delegate for this decoder. This delegate is informed when buffers
  // have been pushed.
  void SetDecoderDelegate(MediaPipelineBackend::Decoder::Delegate* delegate);

  // Pushes a buffer to starboard. Takes ownership of `buffer_data` to manage
  // its lifetime: it will not be freed until Deallocate is called (or this
  // object is destroyed).
  //
  // This function will populate sample_info.drm_info, sample_info.buffer, and
  // sample_info.buffer_size before sending the sample to Starboard. The values
  // will be populated based on the values of `drm_info` and `buffer_data`.
  //
  // `buffer_data` must not be empty (use PushEndOfStream to signal the end of a
  // stream).
  MediaPipelineBackend::BufferStatus PushBufferInternal(
      StarboardSampleInfo sample_info,
      DrmInfoWrapper drm_info,
      std::unique_ptr<uint8_t[]> buffer_data,
      size_t buffer_data_size);

  // Sends an "end of stream" signal to starboard.
  MediaPipelineBackend::BufferStatus PushEndOfStream();

  // Returns the current SbPlayer. May return null if the current SbPlayer has
  // not been set, or if the decoder has been stopped.
  void* GetPlayer();

  // Returns a StarboardApiWrapper, which can be used to interact with Starboard
  // directly.
  StarboardApiWrapper& GetStarboardApi() const;

  // Returns the current delegate, or null if none is currently set.
  MediaPipelineBackend::Decoder::Delegate* GetDelegate() const;

 private:
  // Performs any initialization logic for the child class. This will run after
  // Initialize runs, and is called each time Initialize is called.
  virtual void InitializeInternal() = 0;

  // Runs pending_drm_key_ if `token` matches drm_key_token_. Called once a DRM
  // key is available.
  void RunPendingDrmKeyCallback(int64_t token);

  SEQUENCE_CHECKER(sequence_checker_);
  StarboardApiWrapper* starboard_ = nullptr;
  StarboardMediaType media_type_;
  // The opaque SbPlayer.
  void* player_ = nullptr;
  MediaPipelineBackend::Decoder::Delegate* delegate_ = nullptr;
  // If PushBuffer is called before the decoder has received a handle to the
  // SbPlayer, the push is delayed and this contains the call to
  // PushBufferInternal.
  base::OnceCallback<MediaPipelineBackend::BufferStatus()> pending_first_push_;
  // This map is expected to be small (likely only one or two elements), hence
  // the use of a flat_map.
  // Maps from the array address to the unique_ptr that manages the array.
  base::flat_map<const uint8_t*, std::unique_ptr<uint8_t[]>> copied_buffers_;
  // A callback to be run once the necessary DRM key is available. The callback
  // will push a pending buffer.
  base::OnceCallback<MediaPipelineBackend::BufferStatus()> pending_drm_key_;
  // If we are waiting for a DRM key to be available, this will be set. On
  // destruction, we can use this token to unregister the callback with
  // StarboardDrmKeyTracker. This is not necessary for correct functionality,
  // but helps clean out StarboardDrmKeyTracker's list of callbacks.
  std::optional<int64_t> drm_key_token_;

  // This should be destructed first.
  base::WeakPtrFactory<StarboardDecoder> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_STARBOARD_MEDIA_MEDIA_STARBOARD_DECODER_H_
