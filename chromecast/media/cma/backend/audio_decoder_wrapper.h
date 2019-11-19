// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_DECODER_WRAPPER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_DECODER_WRAPPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/cma/backend/audio_decoder_software_wrapper.h"
#include "chromecast/media/cma/backend/cma_backend.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/public/media/media_pipeline_backend.h"

namespace chromecast {
namespace media {

enum class AudioContentType;

// Make the destructor of CmaBackend::AudioDecoder() public.
class DestructableAudioDecoder : public CmaBackend::AudioDecoder {
 public:
  ~DestructableAudioDecoder() override = default;

  virtual void OnInitialized() = 0;
};

class ActiveAudioDecoderWrapper : public DestructableAudioDecoder {
 public:
  ActiveAudioDecoderWrapper(
      MediaPipelineBackend::AudioDecoder* backend_decoder,
      AudioContentType type,
      MediaPipelineBackendManager::BufferDelegate* buffer_delegate);
  ~ActiveAudioDecoderWrapper() override;

  AudioContentType content_type() const { return content_type_; }

 private:
  // DestructableAudioDecoder implementation:
  void OnInitialized() override;
  void SetDelegate(Delegate* delegate) override;
  BufferStatus PushBuffer(scoped_refptr<DecoderBufferBase> buffer) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;
  void GetStatistics(Statistics* statistics) override;
  bool RequiresDecryption() override;

  AudioDecoderSoftwareWrapper decoder_;
  const AudioContentType content_type_;

  MediaPipelineBackendManager::BufferDelegate* const buffer_delegate_;
  bool initialized_;
  bool delegate_active_;

  float stream_volume_multiplier_;

  scoped_refptr<DecoderBufferBase> pushed_buffer_;

  DISALLOW_COPY_AND_ASSIGN(ActiveAudioDecoderWrapper);
};

class AudioDecoderWrapper : public CmaBackend::AudioDecoder {
 public:
  // Create a functional "real" AudioDecoder.
  AudioDecoderWrapper(
      MediaPipelineBackend::AudioDecoder* backend_decoder,
      AudioContentType type,
      MediaPipelineBackendManager::BufferDelegate* buffer_delegate);
  // Create a "fake" AudioDecoder that's already in revoked state.
  AudioDecoderWrapper(AudioContentType type);
  ~AudioDecoderWrapper() override;

  void OnInitialized();
  void Revoke();

 private:
  // CmaBackend::AudioDecoder implementation:
  void SetDelegate(Delegate* delegate) override;
  BufferStatus PushBuffer(scoped_refptr<DecoderBufferBase> buffer) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  RenderingDelay GetRenderingDelay() override;
  void GetStatistics(Statistics* statistics) override;
  bool RequiresDecryption() override;

  bool decoder_revoked_;

  std::unique_ptr<DestructableAudioDecoder> audio_decoder_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderWrapper);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_DECODER_WRAPPER_H_
