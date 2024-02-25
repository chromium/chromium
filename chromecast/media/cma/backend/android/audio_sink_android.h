// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/cma/backend/android/media_pipeline_backend_android.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

const int kDefaultSlewTimeMs = 15;

class DecoderBufferBase;

// Input handle to the sink. All methods (including constructor and destructor)
// must be called on the same thread.
class AudioSinkAndroid {
 public:
  enum class SinkError {
    // This input is being ignored due to a sample rate changed.
    kInputIgnored,
    // An internal sink error occurred. The input is no longer usable.
    kInternalError,
  };

  class Delegate {
   public:
    using SinkError = AudioSinkAndroid::SinkError;

    // Called when the last data passed to WritePcm() has been successfully
    // added to the queue.
    virtual void OnWritePcmCompletion(
        MediaPipelineBackendAndroid::BufferStatus status) = 0;

    // Called when a sink error occurs. No further data should be written.
    virtual void OnSinkError(SinkError error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  static int64_t GetMinimumBufferedTime(const AudioConfig& config);

  AudioSinkAndroid() {}
  virtual ~AudioSinkAndroid() {}

  // Writes some PCM data to the sink. |data| must be in planar float format.
  // Once the data has been written, the delegate's OnWritePcmCompletion()
  // method will be called on the same thread that the AudioSinkAndroid was
  // created on. Note that no further calls to WritePcm() should be made until
  // OnWritePcmCompletion() has been called.
  virtual void WritePcm(scoped_refptr<DecoderBufferBase> data) = 0;

  // Pauses/unpauses this input.
  virtual void SetPaused(bool paused) = 0;

  // Sets the stream volume multiplier for this input. If |multiplier| is
  // outside the range [0.0, 1.0], it is clamped to that range.
  // The stream volume is not set by the volume controller but rather by the
  // Cast app as an additional volume control on top of Android's.
  virtual void SetStreamVolumeMultiplier(float multiplier) = 0;

  // Sets the limiter multiplier for this input. If |multiplier| is outside the
  // range [0.0, 1.0], it is clamped to that range.
  // The limiter is used by the volume controller to achieve ducking.
  virtual void SetLimiterVolumeMultiplier(float multiplier) = 0;

  // Returns the volume multiplier of the stream, typically the product of
  // stream multiplier and limiter multiplier.
  virtual float EffectiveVolume() const = 0;

  // Returns the current audio rendering delay.
  virtual MediaPipelineBackendAndroid::RenderingDelay GetRenderingDelay() = 0;

  // Returns the current audio track timestamp.
  virtual MediaPipelineBackendAndroid::AudioTrackTimestamp
  GetAudioTrackTimestamp() = 0;

  // Returns the streaming start threshold of the current audio track.
  virtual int GetStartThresholdInFrames() = 0;

  // Getters
  virtual int input_samples_per_second() const = 0;
  virtual bool primary() const = 0;
  virtual std::string device_id() const = 0;
  virtual AudioContentType content_type() const = 0;
};

// Implementation of "managed" AudioSinkAndroid* object that is
// automatically added to the Sink Manager when created and removed when
// destroyed. Inspired by std::unique_ptr<>.
class ManagedAudioSink {
 public:
  using Delegate = AudioSinkAndroid::Delegate;

  ManagedAudioSink();

  ManagedAudioSink(const ManagedAudioSink&) = delete;
  ManagedAudioSink& operator=(const ManagedAudioSink&) = delete;

  ~ManagedAudioSink();

  // Resets the sink_ object by removing it from the manager and deleting it.
  void Reset();

  // Creates a new sink_ object of AudioSinkAndroid* instance and adds it to
  // the manager. If a valid instance existed on entry it is removed from the
  // manager and deleted before creating the new one.
  // Returns true on success; false otherwise.
  bool Create(Delegate* delegate,
              int num_channels,
              int samples_per_second,
              int audio_track_session_id,
              bool primary,
              bool is_apk_audio,
              bool use_hw_av_sync,
              const std::string& device_id,
              AudioContentType content_type) ABSL_MUST_USE_RESULT;

  AudioSinkAndroid* operator->() const { return sink_; }
  operator AudioSinkAndroid*() const { return sink_; }

 private:
  void Remove();

  AudioSinkAndroid* sink_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_H_
