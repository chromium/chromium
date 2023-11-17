// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_CMA_BACKEND_SHIM_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_CMA_BACKEND_SHIM_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/audio/audio_output_service/audio_output_service.pb.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {

namespace media {
class CmaBackendFactory;

namespace audio_output_service {

// A shim to allow the audio output service to use the CMA backend API.
// May be created and used on any thread; internally runs all tasks on the media
// thread. Deletion must be done using Remove() (or using a unique_ptr with
// custom Deleter); Remove() will eventually delete this on the appropriate
// sequence.
class CmaBackendShim : public CmaBackend::AudioDecoder::Delegate {
 public:
  class Delegate {
   public:
    // Called when the CMA backend is initialized (success or failure).
    virtual void OnBackendInitialized(bool success) = 0;

    // Called once data passed to AddData() has been accepted into the queue.
    virtual void OnBufferPushed() = 0;

    // Called when the audio pts changed.
    virtual void UpdateMediaTimeAndRenderingDelay(
        int64_t media_timestamp_microseconds,
        int64_t reference_timestamp_microseconds,
        int64_t delay_microseconds,
        int64_t delay_timestamp_microseconds) = 0;

    // Called if an error occurs in audio playback. No more delegate calls will
    // be made.
    virtual void OnAudioPlaybackError() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Can be used as a custom deleter for unique_ptr.
  struct Deleter {
    void operator()(CmaBackendShim* obj) { obj->Remove(); }
  };

  CmaBackendShim(base::WeakPtr<Delegate> delegate,
                 scoped_refptr<base::SequencedTaskRunner> delegate_task_runner,
                 scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
                 const CmaBackendParams& params,
                 CmaBackendFactory* cma_backend_factory);

  // Removes this audio output. Public methods must not be called after Remove()
  // is called.
  void Remove();

  // Adds more data to be played out. More data should not be passed to
  // AddData() until OnBufferPushed() has been called for the previous AddData()
  // call. |size| is the size of |data| in bytes; a size of 0 indicates
  // end-of-stream.
  void AddData(char* data, int size, int64_t timestamp);

  // Sets the volume multiplier for this stream.
  void SetVolumeMultiplier(float multiplier);

  // Sets the playback rate.
  void SetPlaybackRate(float playback_rate);

  // Starts playback from |start_pts|.
  void StartPlayingFrom(int64_t start_pts);

  // Stops CMA backend.
  void Stop();

  // Updates the config for audio decoder.
  void UpdateAudioConfig(const CmaBackendParams& params);

 private:
  enum class BackendState {
    kStopped,
    kPlaying,
    kPaused,
  };

  ~CmaBackendShim() override;

  // CmaBackend::AudioDecoder::Delegate implementation:
  void OnPushBufferComplete(BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override;
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override;
  void OnVideoResolutionChanged(const Size& size) override;

  void InitializeOnMediaThread();
  void DestroyOnMediaThread();
  void AddEosDataOnMediaThread();
  void AddDataOnMediaThread(scoped_refptr<DecoderBufferBase> buffer);
  void StartPlayingFromOnMediaThread(int64_t start_pts);
  void SetVolumeMultiplierOnMediaThread(float multiplier);
  void SetPlaybackRateOnMediaThread(float playback_rate);
  void StopOnMediaThread();
  void UpdateAudioConfigOnMediaThread(const CmaBackendParams& params);
  bool SetAudioConfig();
  void UpdateMediaTimeAndRenderingDelay();

  const base::WeakPtr<Delegate> delegate_;
  const scoped_refptr<base::SequencedTaskRunner> delegate_task_runner_;
  CmaBackendFactory* const cma_backend_factory_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  TaskRunnerImpl backend_task_runner_;
  CmaBackendParams backend_params_;
  scoped_refptr<DecoderBufferBase> pushed_buffer_;

  float playback_rate_ = 0.0f;

  std::unique_ptr<CmaBackend> cma_backend_;
  BackendState backend_state_ = BackendState::kStopped;
  CmaBackend::AudioDecoder* audio_decoder_;
};

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_CMA_BACKEND_SHIM_H_
