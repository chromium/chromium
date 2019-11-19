// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_MANAGER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

enum class AudioContentType;
class CastDecoderBuffer;
class CmaBackend;
class MediaPipelineBackendWrapper;
class ActiveMediaPipelineBackendWrapper;
class MediaResourceTracker;

// This class tracks all created media backends, tracking whether or not volume
// feedback sounds should be enabled based on the currently active backends.
// Volume feedback sounds are only enabled when there are no active audio
// streams (apart from sound-effects streams).
class MediaPipelineBackendManager {
 public:
  class AllowVolumeFeedbackObserver {
   public:
    virtual void AllowVolumeFeedbackSounds(bool allow) = 0;

   protected:
    virtual ~AllowVolumeFeedbackObserver() = default;
  };

  // Delegate which can process Audio buffers sent to us.
  class BufferDelegate {
   public:
    // Returns |true| if the delegate is accepting buffers.
    virtual bool IsActive() = 0;

    // Called when calls to |OnPushBuffer| will start.
    virtual void OnStreamStarted() = 0;

    // Called when calls to |OnPushBuffer| will stop.
    virtual void OnStreamStopped() = 0;

    // If |IsActive| returns true, the media stream's audio buffers will be sent
    // to the delegate and the media stream's volume will be set to 0.
    //
    // The client may only assume the buffer is in scope during the callback.
    // If the client needs to use the buffer out of scope of the callback (e.g.
    // posted onto a different thread), it must make a copy.
    virtual void OnPushBuffer(const CastDecoderBuffer* buffer) = 0;

    // Called when the media stream's audio config changes. After this call, all
    // subsequent buffers will have the new config. This method will be called
    // regardless if |IsActive| returs true or false.
    virtual void OnSetConfig(const AudioConfig& config) = 0;

    // Called when volume changes. |volume| is from [0.0, 1.0].
    virtual void OnSetVolume(float volume) = 0;

   protected:
    virtual ~BufferDelegate() = default;
  };

  enum DecoderType {
    AUDIO_DECODER,
    VIDEO_DECODER,
    SFX_DECODER,
    NUM_DECODER_TYPES
  };

  MediaPipelineBackendManager(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      MediaResourceTracker* media_resource_tracker);
  ~MediaPipelineBackendManager();

  // Creates a CMA backend. Must be called on the same thread as
  // |media_task_runner_|.
  std::unique_ptr<CmaBackend> CreateCmaBackend(
      const MediaPipelineDeviceParams& params);

  // Inform that a backend previously created is destroyed.
  // Must be called on the same thread as |media_task_runner_|.
  void BackendDestroyed(MediaPipelineBackendWrapper* backend_wrapper);
  // |backend_wrapper| will use a VideoDecoder.
  // MediaPipelineBackendManager needs to record the backend that uses the
  // VideoDecoder; and if there is an active backend using VideoDecoder, that
  // backend needs to be revoked.
  // Must be called on the same thread as |media_task_runner_|.
  void BackendUseVideoDecoder(MediaPipelineBackendWrapper* backend_wrapper);

  base::SingleThreadTaskRunner* task_runner() const {
    return media_task_runner_.get();
  }

  // Adds/removes an observer for when volume feedback sounds are allowed.
  // An observer must be removed on the same thread that added it.
  void AddAllowVolumeFeedbackObserver(AllowVolumeFeedbackObserver* observer);
  void RemoveAllowVolumeFeedbackObserver(AllowVolumeFeedbackObserver* observer);

  // Add/remove a playing audio stream that is not accounted for by a
  // CmaBackend instance. |sfx| indicates whether or not the stream is a sound
  // effects stream (has no effect on volume feedback).
  void AddExtraPlayingStream(bool sfx, const AudioContentType type);
  void RemoveExtraPlayingStream(bool sfx, const AudioContentType type);

  // |buffer_delegate| will get notified for all buffers on the media stream.
  // |buffer_delegate| must outlive |this|.
  // Can only be set once.
  void SetBufferDelegate(BufferDelegate* buffer_delegate);

  BufferDelegate* buffer_delegate() const { return buffer_delegate_; }

  // If |power_save_enabled| is |false|, power save will be turned off and
  // automatic power save will be disabled until this is called with |true|.
  void SetPowerSaveEnabled(bool power_save_enabled);

 private:
  friend class ActiveMediaPipelineBackendWrapper;

  class MixerConnection {
   public:
    virtual ~MixerConnection() = default;
  };

  void CreateMixerConnection();

  // Backend wrapper instances must use these APIs when allocating and releasing
  // decoder objects, so we can enforce global limit on #concurrent decoders.
  bool IncrementDecoderCount(DecoderType type);
  void DecrementDecoderCount(DecoderType type);

  // Update the count of playing non-effects audio streams.
  void UpdatePlayingAudioCount(bool sfx,
                               const AudioContentType type,
                               int change);
  void OnMixerStreamCountChange(int primary_streams, int sfx_streams);
  void HandlePlayingAudioStreamsChange(bool had_playing_audio_streams,
                                       bool prev_allow_feedback);
  int TotalPlayingAudioStreamsCount();
  int TotalPlayingNoneffectsAudioStreamsCount();

  void EnterPowerSaveMode();

  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  MediaResourceTracker* const media_resource_tracker_;

  // Total count of decoders created
  int decoder_count_[NUM_DECODER_TYPES];

  // Total number of playing audio streams.
  base::flat_map<AudioContentType, int> playing_audio_streams_count_;

  // Total number of playing non-effects streams.
  base::flat_map<AudioContentType, int> playing_noneffects_audio_streams_count_;

  scoped_refptr<base::ObserverListThreadSafe<AllowVolumeFeedbackObserver>>
      allow_volume_feedback_observers_;

  // Previously issued MediaPipelineBackendWrapper that uses a video decoder.
  MediaPipelineBackendWrapper* backend_wrapper_using_video_decoder_;

  BufferDelegate* buffer_delegate_;

  bool power_save_enabled_ = true;

  base::OneShotTimer power_save_timer_;

  std::unique_ptr<MixerConnection> mixer_connection_;
  int mixer_primary_stream_count_ = 0;
  int mixer_sfx_stream_count_ = 0;

  base::WeakPtrFactory<MediaPipelineBackendManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPipelineBackendManager);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MEDIA_PIPELINE_BACKEND_MANAGER_H_
