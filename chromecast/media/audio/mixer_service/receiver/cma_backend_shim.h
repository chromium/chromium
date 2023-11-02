// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_RECEIVER_CMA_BACKEND_SHIM_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_RECEIVER_CMA_BACKEND_SHIM_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {
class DecoderBuffer;
}  // namespace media

namespace chromecast {
namespace media {
class MediaPipelineBackendManager;

namespace mixer_service {

// A shim to allow the mixer service to use the CMA backend API if the direct
// audio API is not available. May be created and used on any thread; internally
// runs all tasks on the media thread. Deletion must be done using Remove() (or
// using a unique_ptr with custom Deleter); Remove() will eventually delete this
// on the appropriate sequence.
class CmaBackendShim : public CmaBackend::AudioDecoder::Delegate {
 public:
  using RenderingDelay = CmaBackend::AudioDecoder::RenderingDelay;

  class Delegate {
   public:
    // Called once data passed to AddData() has been accepted into the queue.
    // |rendering_delay| is the estimated rendering delay for the data passed to
    // the next call to AddData(), relative to the clock used for audio
    // timestamping.
    virtual void OnBufferPushed(RenderingDelay rendering_delay) = 0;

    // Called once the end-of-stream has been played out.
    virtual void PlayedEos() = 0;

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
                 const OutputStreamParams& params,
                 MediaPipelineBackendManager* backend_manager);

  CmaBackendShim(const CmaBackendShim&) = delete;
  CmaBackendShim& operator=(const CmaBackendShim&) = delete;

  // Removes this audio output. Public methods must not be called after Remove()
  // is called.
  void Remove();

  // Adds more data to be played out. More data should not be passed to
  // AddData() until OnBufferPushed() has been called for the previous AddData()
  // call. |size| is the size of |data| in bytes; a size of 0 indicates
  // end-of-stream.
  void AddData(char* data, int size);

  // Sets the volume multiplier for this stream.
  void SetVolumeMultiplier(float multiplier);

 private:
  ~CmaBackendShim() override;

  void InitializeOnMediaThread();
  void DestroyOnMediaThread();
  void AddDataOnMediaThread(scoped_refptr<::media::DecoderBuffer> buffer);
  void SetVolumeMultiplierOnMediaThread(float multiplier);

  // CmaBackend::AudioDecoder::Delegate implementation:
  void OnPushBufferComplete(BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override;
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override;
  void OnVideoResolutionChanged(const Size& size) override;

  const base::WeakPtr<Delegate> delegate_;
  const scoped_refptr<base::SequencedTaskRunner> delegate_task_runner_;
  const OutputStreamParams params_;
  MediaPipelineBackendManager* const backend_manager_;
  scoped_refptr<DecoderBufferBase> pushed_buffer_;

  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  TaskRunnerImpl backend_task_runner_;

  SEQUENCE_CHECKER(media_sequence_checker_);

  std::unique_ptr<CmaBackend> cma_backend_;
  CmaBackend::AudioDecoder* audio_decoder_;
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_RECEIVER_CMA_BACKEND_SHIM_H_
