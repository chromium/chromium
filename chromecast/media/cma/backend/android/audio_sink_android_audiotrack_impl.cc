// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/audio_sink_android_audiotrack_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "media/base/audio_bus.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chromecast/media/cma/backend/android/audio_track_jni_headers/AudioSinkAudioTrackImpl_jni.h"

#define RUN_ON_FEEDER_THREAD(callback, ...)                               \
  if (!feeder_task_runner_->BelongsToCurrentThread()) {                   \
    POST_TASK_TO_FEEDER_THREAD(&AudioSinkAndroidAudioTrackImpl::callback, \
                               ##__VA_ARGS__);                            \
    return;                                                               \
  }

#define POST_TASK_TO_FEEDER_THREAD(task, ...) \
  feeder_task_runner_->PostTask(              \
      FROM_HERE, base::BindOnce(task, base::Unretained(this), ##__VA_ARGS__));

#define RUN_ON_CALLER_THREAD(callback, ...)                               \
  if (!caller_task_runner_->BelongsToCurrentThread()) {                   \
    POST_TASK_TO_CALLER_THREAD(&AudioSinkAndroidAudioTrackImpl::callback, \
                               ##__VA_ARGS__);                            \
    return;                                                               \
  }

#define POST_TASK_TO_CALLER_THREAD(task, ...) \
  caller_task_runner_->PostTask(              \
      FROM_HERE,                              \
      base::BindOnce(task, weak_factory_.GetWeakPtr(), ##__VA_ARGS__));

using base::android::JavaParamRef;

namespace chromecast {
namespace media {

// static
int64_t AudioSinkAndroidAudioTrackImpl::GetMinimumBufferedTime(
    int num_channels,
    int samples_per_second) {
  return Java_AudioSinkAudioTrackImpl_getMinimumBufferedTime(
      base::android::AttachCurrentThread(), num_channels, samples_per_second);
}

AudioSinkAndroidAudioTrackImpl::AudioSinkAndroidAudioTrackImpl(
    AudioSinkAndroid::Delegate* delegate,
    int num_channels,
    int input_samples_per_second,
    bool primary,
    bool use_hw_av_sync,
    const std::string& device_id,
    AudioContentType content_type)
    : delegate_(delegate),
      num_channels_(num_channels),
      input_samples_per_second_(input_samples_per_second),
      primary_(primary),
      use_hw_av_sync_(use_hw_av_sync),
      device_id_(device_id),
      content_type_(content_type),
      stream_volume_multiplier_(1.0f),
      limiter_volume_multiplier_(1.0f),
      direct_pcm_buffer_address_(nullptr),
      direct_rendering_delay_address_(nullptr),
      feeder_thread_("AudioTrack feeder thread"),
      feeder_task_runner_(nullptr),
      caller_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      state_(kStateUninitialized) {
  LOG(INFO) << __func__ << "(" << this << "):"
            << " num_channels_=" << num_channels_
            << " input_samples_per_second_=" << input_samples_per_second_
            << " primary_=" << primary_
            << " use_hw_av_sync_=" << use_hw_av_sync_
            << " device_id_=" << device_id_
            << " content_type_=" << content_type_;
  DCHECK(delegate_);
  DCHECK_GT(num_channels_, 0);
  DCHECK_GT(input_samples_per_second_, 0);

  base::Thread::Options options;
  options.thread_type = base::ThreadType::kRealtimeAudio;
  feeder_thread_.StartWithOptions(std::move(options));
  feeder_task_runner_ = feeder_thread_.task_runner();
}

AudioSinkAndroidAudioTrackImpl::~AudioSinkAndroidAudioTrackImpl() {
  LOG(INFO) << __func__ << "(" << this << "): device_id_=" << device_id_;
  PreventDelegateCalls();
  FinalizeOnFeederThread();
  feeder_thread_.Stop();
  feeder_task_runner_ = nullptr;
}

bool AudioSinkAndroidAudioTrackImpl::Initialize(int audio_track_session_id,
                                                bool is_apk_audio) {
  j_audio_sink_audiotrack_impl_ = Java_AudioSinkAudioTrackImpl_create(
      base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this),
      static_cast<int>(content_type_), num_channels_, input_samples_per_second_,
      kDirectBufferSize, audio_track_session_id, is_apk_audio, use_hw_av_sync_);
  if (!j_audio_sink_audiotrack_impl_) {
    return false;
  }

  // Below arguments are initialized from AudioSinkAudioTrackImpl Java class by
  // calling into native APIs of this class.
  DCHECK(direct_pcm_buffer_address_);
  DCHECK(direct_rendering_delay_address_);

  LOG(INFO) << __func__ << "(" << this << "):"
            << " audio_track_session_id=" << audio_track_session_id;
  return true;
}

int AudioSinkAndroidAudioTrackImpl::input_samples_per_second() const {
  return input_samples_per_second_;
}

bool AudioSinkAndroidAudioTrackImpl::primary() const {
  return primary_;
}

std::string AudioSinkAndroidAudioTrackImpl::device_id() const {
  return device_id_;
}

AudioContentType AudioSinkAndroidAudioTrackImpl::content_type() const {
  return content_type_;
}

MediaPipelineBackendAndroid::RenderingDelay
AudioSinkAndroidAudioTrackImpl::GetRenderingDelay() {
  DVLOG(3) << __func__ << "(" << this << "): "
           << " delay=" << sink_rendering_delay_.delay_microseconds
           << " ts=" << sink_rendering_delay_.timestamp_microseconds;
  return sink_rendering_delay_;
}

MediaPipelineBackendAndroid::AudioTrackTimestamp
AudioSinkAndroidAudioTrackImpl::GetAudioTrackTimestamp() {
  // TODO(ziyangch): Add a rate limiter to avoid calling AudioTrack.getTimestamp
  // too frequent.
  Java_AudioSinkAudioTrackImpl_getAudioTrackTimestamp(
      base::android::AttachCurrentThread(), j_audio_sink_audiotrack_impl_);
  return MediaPipelineBackendAndroid::AudioTrackTimestamp(
      direct_audio_track_timestamp_address_[0],
      direct_audio_track_timestamp_address_[1],
      direct_audio_track_timestamp_address_[2]);
}

int AudioSinkAndroidAudioTrackImpl::GetStartThresholdInFrames() {
  return Java_AudioSinkAudioTrackImpl_getStartThresholdInFrames(
      base::android::AttachCurrentThread(), j_audio_sink_audiotrack_impl_);
}

void AudioSinkAndroidAudioTrackImpl::FinalizeOnFeederThread() {
  RUN_ON_FEEDER_THREAD(FinalizeOnFeederThread);
  wait_for_eos_task_.Cancel();

  if (j_audio_sink_audiotrack_impl_) {
    Java_AudioSinkAudioTrackImpl_close(base::android::AttachCurrentThread(),
                                       j_audio_sink_audiotrack_impl_);
  }
}

void AudioSinkAndroidAudioTrackImpl::PreventDelegateCalls() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();
}

void AudioSinkAndroidAudioTrackImpl::CacheDirectBufferAddress(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& pcm_byte_buffer,
    const JavaParamRef<jobject>& rendering_delay_byte_buffer,
    const JavaParamRef<jobject>& audio_track_timestamp_byte_buffer) {
  direct_pcm_buffer_address_ =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(pcm_byte_buffer));
  direct_rendering_delay_address_ = static_cast<uint64_t*>(
      env->GetDirectBufferAddress(rendering_delay_byte_buffer));
  direct_audio_track_timestamp_address_ = static_cast<uint64_t*>(
      env->GetDirectBufferAddress(audio_track_timestamp_byte_buffer));
}

void AudioSinkAndroidAudioTrackImpl::WritePcm(
    scoped_refptr<DecoderBufferBase> data) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_data_);
  pending_data_ = std::move(data);
  pending_data_bytes_already_fed_ = 0;
  FeedData();
}

void AudioSinkAndroidAudioTrackImpl::FeedData() {
  RUN_ON_FEEDER_THREAD(FeedData);

  DCHECK(pending_data_);
  if (pending_data_->end_of_stream()) {
    state_ = kStateGotEos;
    ScheduleWaitForEosTask();
    return;
  }

  if (pending_data_->data_size() == 0) {
    LOG(INFO) << __func__ << "(" << this << "): empty data buffer!";
    PostPcmCallback();
    return;
  }

  pending_data_bytes_after_reformat_ = ReformatData();
  DVLOG(3) << __func__ << "(" << this << "):"
           << " [" << pending_data_bytes_after_reformat_ << "]"
           << " @ts=" << pending_data_->timestamp();

  int written = Java_AudioSinkAudioTrackImpl_writePcm(
      base::android::AttachCurrentThread(), j_audio_sink_audiotrack_impl_,
      pending_data_bytes_after_reformat_,
      (pending_data_->timestamp() == INT64_MIN)
          ? pending_data_->timestamp()
          : pending_data_->timestamp() *
                base::Time::kNanosecondsPerMicrosecond);

  if (written < 0) {
    LOG(ERROR) << __func__ << "(" << this << "): Cannot write PCM via JNI!";
    SignalError(AudioSinkAndroid::SinkError::kInternalError);
    return;
  }

  if (state_ == kStatePaused && written < pending_data_bytes_after_reformat_) {
    LOG(INFO) << "Audio Server is full while in PAUSED, "
              << "will continue when entering PLAY mode.";
    pending_data_bytes_already_fed_ = written;
    return;
  }

  if (written != pending_data_bytes_after_reformat_) {
    LOG(ERROR) << __func__ << "(" << this << "): Wrote " << written
               << " instead of " << pending_data_bytes_after_reformat_;
    // continue anyway, better to do a best-effort than fail completely
  }

  // RenderingDelay was returned through JNI via direct buffers.
  sink_rendering_delay_.delay_microseconds = direct_rendering_delay_address_[0];
  sink_rendering_delay_.timestamp_microseconds =
      direct_rendering_delay_address_[1];

  TrackRawMonotonicClockDeviation();

  PostPcmCallback();
}

void AudioSinkAndroidAudioTrackImpl::ScheduleWaitForEosTask() {
  DCHECK(wait_for_eos_task_.IsCancelled());
  DCHECK(state_ == kStateGotEos);
  DCHECK(feeder_task_runner_->BelongsToCurrentThread());

  int64_t playout_time_left_us =
      Java_AudioSinkAudioTrackImpl_prepareForShutdown(
          base::android::AttachCurrentThread(), j_audio_sink_audiotrack_impl_);
  LOG(INFO) << __func__ << "(" << this << "): Hit EOS, playout time left is "
            << playout_time_left_us << "us";
  wait_for_eos_task_.Reset(base::BindOnce(
      &AudioSinkAndroidAudioTrackImpl::OnPlayoutDone, base::Unretained(this)));
  base::TimeDelta delay = base::Microseconds(playout_time_left_us);
  feeder_task_runner_->PostDelayedTask(FROM_HERE, wait_for_eos_task_.callback(),
                                       delay);
}

void AudioSinkAndroidAudioTrackImpl::OnPlayoutDone() {
  DCHECK(feeder_task_runner_->BelongsToCurrentThread());
  DCHECK(state_ == kStateGotEos);
  PostPcmCallback();
}

int AudioSinkAndroidAudioTrackImpl::ReformatData() {
  // Data is in planar float format, i.e., planar audio data for stereo is all
  // left samples first, then all right -> "LLLLLLLLLLLLLLLLRRRRRRRRRRRRRRRR").
  // AudioTrack needs interleaved format -> "LRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLR").
  DCHECK(direct_pcm_buffer_address_);
  DCHECK_EQ(0, static_cast<int>(pending_data_->data_size() % sizeof(float)));
  CHECK_LT(static_cast<int>(pending_data_->data_size()), kDirectBufferSize);
  int num_of_samples = pending_data_->data_size() / sizeof(float);
  int num_of_frames = num_of_samples / num_channels_;
  std::vector<const float*> src(num_channels_);
  for (int c = 0; c < num_channels_; c++) {
    src[c] = reinterpret_cast<const float*>(pending_data_->data()) +
             c * num_of_frames;
  }
  if (use_hw_av_sync_) {
    // Convert audio data from float to int16_t since hardware av sync audio
    // track requires ENCODING_PCM_16BIT audio format.
    int16_t* dst = reinterpret_cast<int16_t*>(direct_pcm_buffer_address_);
    for (int f = 0; f < num_of_frames; f++) {
      for (int c = 0; c < num_channels_; c++) {
        *dst++ = *src[c]++;
      }
    }
    return static_cast<int>(pending_data_->data_size()) /
           (sizeof(float) / sizeof(int16_t));
  } else {
    float* dst = reinterpret_cast<float*>(direct_pcm_buffer_address_);
    for (int f = 0; f < num_of_frames; f++) {
      for (int c = 0; c < num_channels_; c++) {
        *dst++ = *src[c]++;
      }
    }
    return static_cast<int>(pending_data_->data_size());
  }
}

void AudioSinkAndroidAudioTrackImpl::TrackRawMonotonicClockDeviation() {
  timespec now = {0, 0};
  clock_gettime(CLOCK_MONOTONIC, &now);
  int64_t now_usec = base::TimeDelta::FromTimeSpec(now).InMicroseconds();

  clock_gettime(CLOCK_MONOTONIC_RAW, &now);
  int64_t now_raw_usec = base::TimeDelta::FromTimeSpec(now).InMicroseconds();

  // TODO(ckuiper): Eventually we want to use this to convert from non-RAW to
  // RAW timestamps to improve accuracy.
  DVLOG(3) << __func__ << "(" << this << "):"
           << " now - now_raw=" << (now_usec - now_raw_usec);
}

void AudioSinkAndroidAudioTrackImpl::FeedDataContinue() {
  RUN_ON_FEEDER_THREAD(FeedDataContinue);

  DCHECK(pending_data_);
  DCHECK(pending_data_bytes_already_fed_);

  int left_to_send =
      pending_data_bytes_after_reformat_ - pending_data_bytes_already_fed_;
  LOG(INFO) << __func__ << "(" << this << "): send remaining " << left_to_send
            << "/" << pending_data_bytes_after_reformat_;

  memmove(direct_pcm_buffer_address_,
          direct_pcm_buffer_address_ + pending_data_bytes_already_fed_,
          left_to_send);

  int bytes_per_frame =
      num_channels_ * (use_hw_av_sync_ ? sizeof(int16_t) : sizeof(float));
  int64_t fed_frames = pending_data_bytes_already_fed_ / bytes_per_frame;
  int64_t timestamp_ns_new =
      (pending_data_->timestamp() == INT64_MIN)
          ? pending_data_->timestamp()
          : pending_data_->timestamp() *
                    base::Time::kNanosecondsPerMicrosecond +
                fed_frames * base::Time::kNanosecondsPerSecond /
                    input_samples_per_second_;
  int written = Java_AudioSinkAudioTrackImpl_writePcm(
      base::android::AttachCurrentThread(), j_audio_sink_audiotrack_impl_,
      left_to_send, timestamp_ns_new);

  DCHECK(written == left_to_send);

  // RenderingDelay was returned through JNI via direct buffers.
  sink_rendering_delay_.delay_microseconds = direct_rendering_delay_address_[0];
  sink_rendering_delay_.timestamp_microseconds =
      direct_rendering_delay_address_[1];

  TrackRawMonotonicClockDeviation();

  PostPcmCallback();
}

void AudioSinkAndroidAudioTrackImpl::PostPcmCallback() {
  RUN_ON_CALLER_THREAD(PostPcmCallback);
  DCHECK(pending_data_);
  pending_data_ = nullptr;
  pending_data_bytes_already_fed_ = 0;
  delegate_->OnWritePcmCompletion(MediaPipelineBackendAndroid::kBufferSuccess);
}

void AudioSinkAndroidAudioTrackImpl::SignalError(
    AudioSinkAndroid::SinkError error) {
  DCHECK(feeder_task_runner_->BelongsToCurrentThread());
  state_ = kStateError;
  PostError(error);
}

void AudioSinkAndroidAudioTrackImpl::PostError(
    AudioSinkAndroid::SinkError error) {
  RUN_ON_CALLER_THREAD(PostError, error);
  delegate_->OnSinkError(error);
}

void AudioSinkAndroidAudioTrackImpl::SetPaused(bool paused) {
  RUN_ON_FEEDER_THREAD(SetPaused, paused);

  if (paused) {
    LOG(INFO) << __func__ << "(" << this << "): Pausing";
    state_ = kStatePaused;
    Java_AudioSinkAudioTrackImpl_pause(base::android::AttachCurrentThread(),
                                       j_audio_sink_audiotrack_impl_);
  } else {
    LOG(INFO) << __func__ << "(" << this << "): Unpausing";
    sink_rendering_delay_ = MediaPipelineBackendAndroid::RenderingDelay();
    state_ = kStateNormalPlayback;
    Java_AudioSinkAudioTrackImpl_play(base::android::AttachCurrentThread(),
                                      j_audio_sink_audiotrack_impl_);
    if (pending_data_ && pending_data_bytes_already_fed_) {
      // The last data buffer was partially fed, complete it now.
      FeedDataContinue();
    }
  }
}

void AudioSinkAndroidAudioTrackImpl::UpdateVolume() {
  DCHECK(feeder_task_runner_->BelongsToCurrentThread());
  Java_AudioSinkAudioTrackImpl_setVolume(base::android::AttachCurrentThread(),
                                         j_audio_sink_audiotrack_impl_,
                                         EffectiveVolume());
}

void AudioSinkAndroidAudioTrackImpl::SetStreamVolumeMultiplier(
    float multiplier) {
  RUN_ON_FEEDER_THREAD(SetStreamVolumeMultiplier, multiplier);

  stream_volume_multiplier_ = std::max(0.0f, multiplier);
  LOG(INFO) << __func__ << "(" << this << "): device_id_=" << device_id_
            << " stream_multiplier=" << stream_volume_multiplier_
            << " effective=" << EffectiveVolume();
  UpdateVolume();
}

void AudioSinkAndroidAudioTrackImpl::SetLimiterVolumeMultiplier(
    float multiplier) {
  RUN_ON_FEEDER_THREAD(SetLimiterVolumeMultiplier, multiplier);

  limiter_volume_multiplier_ = std::clamp(multiplier, 0.0f, 1.0f);
  LOG(INFO) << __func__ << "(" << this << "): device_id_=" << device_id_
            << " limiter_multiplier=" << limiter_volume_multiplier_
            << " effective=" << EffectiveVolume();
  UpdateVolume();
}

float AudioSinkAndroidAudioTrackImpl::EffectiveVolume() const {
  return stream_volume_multiplier_ * limiter_volume_multiplier_;
}

}  // namespace media
}  // namespace chromecast
