// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CMA_AUDIO_OUTPUT_STREAM_H_
#define CHROMECAST_MEDIA_AUDIO_CMA_AUDIO_OUTPUT_STREAM_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/media/api/cma_backend.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace media {
class AudioBus;
}  // namespace media

namespace chromecast {

class TaskRunnerImpl;

namespace media {

class CmaAudioOutput;
class CmaBackendFactory;

class CmaAudioOutputStream : public CmaBackend::Decoder::Delegate {
 public:
  CmaAudioOutputStream(const ::media::AudioParameters& audio_params,
                       base::TimeDelta buffer_duration,
                       const std::string& device_id,
                       CmaBackendFactory* cma_backend_factory);

  CmaAudioOutputStream(const CmaAudioOutputStream&) = delete;
  CmaAudioOutputStream& operator=(const CmaAudioOutputStream&) = delete;

  ~CmaAudioOutputStream() override;

  void SetRunning(bool running);
  void Initialize(const std::string& application_session_id);
  void Start(::media::AudioOutputStream::AudioSourceCallback* source_callback);
  void Stop(base::WaitableEvent* finished);
  void Flush(base::WaitableEvent* finished);
  void Close(base::OnceClosure closure);
  void SetVolume(double volume);

 private:
  enum class CmaBackendState {
    kUninitialized,
    kStopped,
    kPaused,
    kStarted,
    kPendingClose,
  };

  void PushBuffer();

  // CmaBackend::Decoder::Delegate implementation:
  void OnEndOfStream() override {}
  void OnDecoderError() override;
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override {}
  void OnVideoResolutionChanged(const Size& size) override {}
  void OnPushBufferComplete(BufferStatus status) override;

  const bool is_audio_prefetch_;

  const ::media::AudioParameters audio_params_;
  const std::string device_id_;
  CmaBackendFactory* const cma_backend_factory_;
  std::unique_ptr<CmaAudioOutput> output_;

  base::Lock running_lock_;
  bool running_ = true;
  CmaBackendState cma_backend_state_ = CmaBackendState::kUninitialized;
  const base::TimeDelta buffer_duration_;
  std::unique_ptr<::media::AudioBus> audio_bus_;
  base::OneShotTimer push_timer_;
  bool push_in_progress_ = false;
  bool encountered_error_ = false;
  base::TimeTicks next_push_time_;
  base::TimeTicks last_push_complete_time_;
  base::TimeDelta last_rendering_delay_;
  base::TimeDelta render_buffer_size_estimate_;
  ::media::AudioOutputStream::AudioSourceCallback* source_callback_ = nullptr;

  THREAD_CHECKER(media_thread_checker_);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CMA_AUDIO_OUTPUT_STREAM_H_
