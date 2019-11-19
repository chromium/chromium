// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_AUDIOTRACK_IMPL_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_AUDIOTRACK_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "chromecast/media/cma/backend/android/audio_sink_android.h"
#include "chromecast/media/cma/backend/android/media_pipeline_backend_android.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {

class AudioSinkAndroidAudioTrackImpl : public AudioSinkAndroid {
 public:
  enum State {
    kStateUninitialized,   // No data has been queued yet.
    kStateNormalPlayback,  // Normal playback.
    kStatePaused,          // Currently paused.
    kStateGotEos,          // Got the end-of-stream buffer (normal playback).
    kStateError,           // A sink error occurred, this is unusable now.
  };

  // TODO(ckuiper): There doesn't seem to be a maximum size for the buffers
  // sent through the media pipeline, so we need to add code to break up a
  // buffer larger than this size and feed it in in smaller chunks.
  static const int kDirectBufferSize = 512 * 1024;

  // Gets the Android audio session ids used for media and communication (TTS)
  // tracks.
  // Set a return value pointer to null if that id is not needed.
  // Returns true if the ids populated are valid.
  static bool GetSessionIds(int* media_id, int* communication_id);

  static int64_t GetMinimumBufferedTime(int num_channels,
                                        int samples_per_second);

  // Called from Java so that we can cache the addresses of the Java-managed
  // byte_buffers.
  void CacheDirectBufferAddress(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& pcm_byte_buffer,
      const base::android::JavaParamRef<jobject>& timestamp_byte_buffer);

  // AudioSinkAndroid implementation
  void WritePcm(scoped_refptr<DecoderBufferBase> data) override;
  void SetPaused(bool paused) override;
  void SetStreamVolumeMultiplier(float multiplier) override;
  void SetLimiterVolumeMultiplier(float multiplier) override;
  float EffectiveVolume() const override;

  // Getters
  int input_samples_per_second() const override;
  bool primary() const override;
  std::string device_id() const override;
  AudioContentType content_type() const override;
  const char* GetContentTypeName() const override;

  // Prevents any further calls to the delegate (ie, called when the delegate
  // is being destroyed).
  void PreventDelegateCalls();

  State state() const { return state_; }

 private:
  friend class ManagedAudioSink;

  AudioSinkAndroidAudioTrackImpl(AudioSinkAndroid::Delegate* delegate,
                                 int num_channels,
                                 int input_samples_per_second,
                                 bool primary,
                                 const std::string& device_id,
                                 AudioContentType content_type);

  ~AudioSinkAndroidAudioTrackImpl() override;

  void FinalizeOnFeederThread();

  void FeedData();
  void FeedDataContinue();

  void ScheduleWaitForEosTask();
  void OnPlayoutDone();

  // Reformats audio data from planar float into interleaved float for
  // AudioTrack. I.e.:
  // "LLLLLLLLLLLLLLLLRRRRRRRRRRRRRRRR" -> "LRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLR".
  void ReformatData();

  void TrackRawMonotonicClockDeviation();

  void PostPcmCallback(
      const MediaPipelineBackendAndroid::RenderingDelay& delay);

  void SignalError(AudioSinkAndroid::SinkError error);
  void PostError(AudioSinkAndroid::SinkError error);

  void UpdateVolume();

  // Config parameters provided into c'tor.
  Delegate* const delegate_;
  const int num_channels_;
  const int input_samples_per_second_;
  const bool primary_;
  const std::string device_id_;
  const AudioContentType content_type_;

  float stream_volume_multiplier_;
  float limiter_volume_multiplier_;

  // Java AudioSinkAudioTrackImpl instance.
  base::android::ScopedJavaGlobalRef<jobject> j_audio_sink_audiotrack_impl_;

  // Thread that feeds audio data into the Java instance though JNI,
  // potentially blocking. When in Play mode the Java AudioTrack blocks as it
  // waits for queue space to become available for the new data. In Pause mode
  // it returns immediately once all queue space has been filled up. This case
  // is handled separately via FeedDataContinue().
  base::Thread feeder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> feeder_task_runner_;

  base::CancelableClosure wait_for_eos_task_;

  const scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Buffers shared between native and Java space to move data across the JNI.
  // We use direct buffers so that the native class can have access to the
  // underlying memory address. This avoids the need to copy from a jbyteArray
  // to native memory. More discussion of this here:
  // http://developer.android.com/training/articles/perf-jni.html Owned by
  // j_audio_sink_audiotrack_impl_.
  uint8_t* direct_pcm_buffer_address_;  // PCM audio data native->java
  // rendering delay+timestamp return value, java->native
  uint64_t* direct_rendering_delay_address_;

  State state_;

  scoped_refptr<DecoderBufferBase> pending_data_;
  int pending_data_bytes_already_fed_;

  MediaPipelineBackendAndroid::RenderingDelay sink_rendering_delay_;

  base::WeakPtr<AudioSinkAndroidAudioTrackImpl> weak_this_;
  base::WeakPtrFactory<AudioSinkAndroidAudioTrackImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioSinkAndroidAudioTrackImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_ANDROID_AUDIO_SINK_ANDROID_AUDIOTRACK_IMPL_H_
