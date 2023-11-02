// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_PROXY_AUDIO_DECODER_PIPELINE_NODE_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_PROXY_AUDIO_DECODER_PIPELINE_NODE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "chromecast/media/api/cma_backend.h"
#include "chromecast/public/media/cast_key_status.h"

namespace chromecast {

struct Size;

namespace media {

// This class acts as a single node in a series of Audio Decoders which form an
// audio processing pipeline. This can be thought of as a tree with the
// following characteristics:
// - Each node has at most one child.
// - The single leaf node is a CmaBackend::AudioDecoder.
// - All other nodes are instances of this class.
// By default, all calls to this class's CmaBackend::AudioDecoder nodes are
// passed back to this node's child. Additionally, if SetDelegate() is called,
// any calls to this class's CmaBackendDecoder::Delegate methods are passed to
// the set delegate, forming a pipeline in the opposite order. All methods are
// expected to be called on the same thread.
class AudioDecoderPipelineNode : public CmaBackend::Decoder::Delegate,
                                 public CmaBackend::AudioDecoder {
 public:
  explicit AudioDecoderPipelineNode(
      CmaBackend::AudioDecoder* delegated_decoder);
  AudioDecoderPipelineNode(const AudioDecoderPipelineNode& other) = delete;

  ~AudioDecoderPipelineNode() override;

  AudioDecoderPipelineNode& operator=(const AudioDecoderPipelineNode& other) =
      delete;

  // CmaBackend::AudioDecoder overrides.
  void SetDelegate(CmaBackend::Decoder::Delegate* delegate) override;
  CmaBackend::Decoder::BufferStatus PushBuffer(
      scoped_refptr<DecoderBufferBase> buffer) override;
  bool SetConfig(const AudioConfig& config) override;
  bool SetVolume(float multiplier) override;
  CmaBackend::AudioDecoder::RenderingDelay GetRenderingDelay() override;
  void GetStatistics(CmaBackend::AudioDecoder::Statistics* statistics) override;
  CmaBackend::AudioDecoder::AudioTrackTimestamp GetAudioTrackTimestamp() override;
  int GetStartThresholdInFrames() override;
  bool RequiresDecryption() override;

 protected:
  inline void CheckCalledOnCorrectThread() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  // Sets the decoder to be owned by this node of the pipeline. Must match the
  // instance provided as |delegated_decoder_| in the ctor.
  void SetOwnedDecoder(
      std::unique_ptr<AudioDecoderPipelineNode> delegated_decoder);

  // CmaBackend::Decoder::Delegate overrides.
  void OnPushBufferComplete(CmaBackend::Decoder::BufferStatus status) override;
  void OnEndOfStream() override;
  void OnDecoderError() override;
  void OnKeyStatusChanged(const std::string& key_id,
                          CastKeyStatus key_status,
                          uint32_t system_code) override;
  void OnVideoResolutionChanged(const Size& size) override;

 private:
  friend class AudioDecoderPipelineNodeTests;

  // Local storage for the object backing |delegated_decoder_|, when it is
  // owned by this class. Empty unless set by SetOwnedDecoder().
  std::unique_ptr<AudioDecoderPipelineNode> owned_delegated_decoder_;

  // Decoder::Delegate to which all calls to CmaBackend::Decoder::Delegate
  // methods should be forwarded.
  CmaBackend::Decoder::Delegate* delegated_decoder_delegate_;

  // Decoder to which all calls to CmaBackend::AudioDecoder methods will be
  // forwarded.
  CmaBackend::AudioDecoder* const delegated_decoder_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_PROXY_AUDIO_DECODER_PIPELINE_NODE_H_
