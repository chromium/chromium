// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_AV_PIPELINE_IMPL_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_AV_PIPELINE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/media/cma/pipeline/av_pipeline_client.h"
#include "chromecast/media/cma/pipeline/stream_decryptor.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "chromecast/public/media/stream_id.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/pipeline_status.h"

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}

namespace chromecast {
namespace media {
class CastCdmContext;
class BufferingFrameProvider;
class BufferingState;
class CodedFrameProvider;
class DecoderBufferBase;

class AvPipelineImpl : CmaBackend::Decoder::Delegate {
 public:
  AvPipelineImpl(CmaBackend::Decoder* decoder, AvPipelineClient client);

  AvPipelineImpl(const AvPipelineImpl&) = delete;
  AvPipelineImpl& operator=(const AvPipelineImpl&) = delete;

  ~AvPipelineImpl() override;

  void SetCdm(CastCdmContext* cast_cdm_context);

  // Setup the pipeline and ensure samples are available for the given media
  // time, then start rendering samples.
  bool StartPlayingFrom(base::TimeDelta time,
                        const scoped_refptr<BufferingState>& buffering_state);
  void Flush(base::OnceClosure flush_cb);

  virtual void UpdateStatistics() = 0;

  int bytes_decoded_since_last_update() const {
    return bytes_decoded_since_last_update_;
  }

 protected:
  // Pipeline states.
  enum State {
    kUninitialized,
    kPlaying,
    kFlushing,
    kFlushed,
    kError,
  };

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }
  const AvPipelineClient& client() const { return client_; }
  CastCdmContext* cdm_context() const { return cast_cdm_context_; }

  virtual void OnUpdateConfig(
      StreamId id,
      const ::media::AudioDecoderConfig& audio_config,
      const ::media::VideoDecoderConfig& video_config) = 0;
  virtual EncryptionScheme GetEncryptionScheme(StreamId id) const = 0;

  // Create a decoder for decrypt and decode.
  virtual std::unique_ptr<StreamDecryptor> CreateDecryptor() = 0;

  // Setting the frame provider must be done in the |kUninitialized| state.
  void SetCodedFrameProvider(std::unique_ptr<CodedFrameProvider> frame_provider,
                             size_t max_buffer_size,
                             size_t max_frame_size);

  ::media::PipelineStatistics previous_stats_;
  int bytes_decoded_since_last_update_;

 private:
  void OnFlushDone();

  // CmaBackend::Decoder::Delegate implementation:
  void OnPushBufferComplete(BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override;
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override;
  void OnVideoResolutionChanged(const Size& size) override;

  void OnBufferDecrypted(bool success, StreamDecryptor::BufferQueue buffers);

  // Feed the pipeline, getting the frames from |frame_provider_|.
  void FetchBuffer();

  // Callback invoked when receiving a new frame from |frame_provider_|.
  void OnNewFrame(const scoped_refptr<DecoderBufferBase>& buffer,
                  const ::media::AudioDecoderConfig& audio_config,
                  const ::media::VideoDecoderConfig& video_config);

  // Process a pending buffer.
  void ProcessPendingBuffer();
  void DoPushBufferCompleteTask(CmaBackend::BufferStatus status);

  // Pushes all the ready buffers to decoder.
  void PushAllReadyBuffers();

  // Pushes one ready buffer to decoder.
  void PushReadyBuffer(scoped_refptr<DecoderBufferBase> buffer);

  // Callbacks when CastCdm updated its state.
  void OnCdmStateChanged(::media::CdmContext::Event event);

  // Callback invoked when a media buffer has been buffered by |frame_provider_|
  // which is a BufferingFrameProvider.
  void OnDataBuffered(const scoped_refptr<DecoderBufferBase>& buffer,
                      bool is_at_max_capacity);
  void UpdatePlayableFrames();

  std::unique_ptr<StreamDecryptor> CreateStreamDecryptor(
      CastKeySystem key_system);

  base::ThreadChecker thread_checker_;

  CmaBackend::Decoder* const decoder_;
  AvPipelineClient client_;

  // Callback provided to Flush().
  base::OnceClosure flush_cb_;

  // AV pipeline state.
  State state_;

  // Buffering state.
  // Can be NULL if there is no buffering strategy.
  scoped_refptr<BufferingState> buffering_state_;

  // |buffered_time_| is the maximum timestamp of buffered frames.
  // |playable_buffered_time_| is the maximum timestamp of buffered and
  // playable frames (i.e. the key id is available for those frames).
  base::TimeDelta buffered_time_;
  base::TimeDelta playable_buffered_time_;

  // List of frames buffered but not playable right away due to a missing
  // key id.
  std::list<scoped_refptr<DecoderBufferBase> > non_playable_frames_;

  // Buffer provider.
  std::unique_ptr<BufferingFrameProvider> frame_provider_;

  // Indicate whether the frame fetching process is active.
  bool enable_feeding_;

  // Indicate whether there is a pending buffer read.
  bool pending_read_;

  // Pending buffer (not pushed to decryptor yet)
  scoped_refptr<DecoderBufferBase> pending_buffer_;

  // Buffers which are ready to be pushed to decoder.
  StreamDecryptor::BufferQueue ready_buffers_;

  // Buffer that has been pushed to the device but not processed yet.
  scoped_refptr<DecoderBufferBase> pushed_buffer_;

  // CdmContext, if available.
  CastCdmContext* cast_cdm_context_;

  // To keep the CdmContext event callback registered.
  std::unique_ptr<::media::CallbackRegistration> event_cb_registration_;

  // Decryptor to get clear buffers. All the buffers (clear or encrypted) will
  // be pushed to |decryptor_| before being pushed to |decoder_|. |decryptor_|
  // can do nothing if the media backend is able to handle encrypted buffer.
  std::unique_ptr<StreamDecryptor> decryptor_;

  base::WeakPtr<AvPipelineImpl> weak_this_;
  base::WeakPtrFactory<AvPipelineImpl> weak_factory_;
  // Special weak factory used for asynchronous decryption. This allows us to
  // cancel pending asynchronous decryption (by invalidating this factory's weak
  // ptrs) without affecting other bound callbacks.
  base::WeakPtrFactory<AvPipelineImpl> decrypt_weak_factory_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_AV_PIPELINE_IMPL_H_
