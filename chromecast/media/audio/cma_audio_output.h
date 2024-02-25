// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CMA_AUDIO_OUTPUT_H_
#define CHROMECAST_MEDIA_AUDIO_CMA_AUDIO_OUTPUT_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"

namespace chromecast {

class TaskRunnerImpl;

namespace media {

class CastDecoderBufferImpl;
class CmaBackendFactory;

class CmaAudioOutput {
 public:
  CmaAudioOutput(const ::media::AudioParameters& audio_params,
                 SampleFormat sample_format,
                 const std::string& device_id,
                 const std::string& application_session_id,
                 MediaPipelineDeviceParams::MediaSyncType sync_type,
                 bool use_hw_av_sync,
                 int audio_track_session_id,
                 CmaBackendFactory* cma_backend_factory,
                 CmaBackend::Decoder::Delegate* delegate);
  // Disallow copy and assign.
  CmaAudioOutput(const CmaAudioOutput&) = delete;
  CmaAudioOutput& operator=(const CmaAudioOutput&) = delete;
  ~CmaAudioOutput();

  bool Start(int64_t start_pts);
  void Stop();
  bool Pause();
  bool Resume();
  bool SetVolume(double volume);

  void PushBuffer(scoped_refptr<CastDecoderBufferImpl> decoder_buffer,
                  bool is_silence);
  CmaBackend::AudioDecoder::RenderingDelay GetRenderingDelay();
  CmaBackend::AudioDecoder::AudioTrackTimestamp GetAudioTrackTimestamp();
  int64_t GetTotalFrames();

 private:
  void Initialize(SampleFormat sample_format,
                  const std::string& device_id,
                  const std::string& application_session_id,
                  MediaPipelineDeviceParams::MediaSyncType sync_type,
                  int audio_track_session_id,
                  CmaBackendFactory* cma_backend_factory);

  const ::media::AudioParameters audio_params_;
  const int sample_size_;
  const bool use_hw_av_sync_;
  CmaBackend::Decoder::Delegate* const delegate_;

  ::media::AudioTimestampHelper timestamp_helper_;
  std::unique_ptr<TaskRunnerImpl> cma_backend_task_runner_;
  std::unique_ptr<CmaBackend> cma_backend_;
  CmaBackend::AudioDecoder* audio_decoder_ = nullptr;

  THREAD_CHECKER(media_thread_checker_);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CMA_AUDIO_OUTPUT_H_
