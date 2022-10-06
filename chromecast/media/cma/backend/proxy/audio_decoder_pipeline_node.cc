// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/audio_decoder_pipeline_node.h"

#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

AudioDecoderPipelineNode::AudioDecoderPipelineNode(
    CmaBackend::AudioDecoder* delegated_decoder)
    : delegated_decoder_(delegated_decoder) {
  DCHECK(delegated_decoder_);
}

AudioDecoderPipelineNode::~AudioDecoderPipelineNode() = default;

void AudioDecoderPipelineNode::SetDelegate(
    CmaBackend::Decoder::Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  delegated_decoder_delegate_ = delegate;
  delegated_decoder_->SetDelegate(this);
}

CmaBackend::Decoder::BufferStatus AudioDecoderPipelineNode::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return delegated_decoder_->PushBuffer(std::move(buffer));
}

bool AudioDecoderPipelineNode::SetConfig(const AudioConfig& config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return delegated_decoder_->SetConfig(config);
}

bool AudioDecoderPipelineNode::SetVolume(float multiplier) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return delegated_decoder_->SetVolume(multiplier);
}

CmaBackend::AudioDecoder::RenderingDelay
AudioDecoderPipelineNode::GetRenderingDelay() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return delegated_decoder_->GetRenderingDelay();
}

void AudioDecoderPipelineNode::GetStatistics(
    CmaBackend::AudioDecoder::Statistics* statistics) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return delegated_decoder_->GetStatistics(statistics);
}

CmaBackend::AudioDecoder::AudioTrackTimestamp
AudioDecoderPipelineNode::GetAudioTrackTimestamp() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return delegated_decoder_->GetAudioTrackTimestamp();
}

int AudioDecoderPipelineNode::GetStartThresholdInFrames() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return delegated_decoder_->GetStartThresholdInFrames();
}

bool AudioDecoderPipelineNode::RequiresDecryption() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return delegated_decoder_->RequiresDecryption();
}

void AudioDecoderPipelineNode::SetOwnedDecoder(
    std::unique_ptr<AudioDecoderPipelineNode> delegated_decoder) {
  DCHECK_EQ(delegated_decoder.get(), delegated_decoder_);

  owned_delegated_decoder_ = std::move(delegated_decoder);
}

void AudioDecoderPipelineNode::OnPushBufferComplete(
    CmaBackend::Decoder::BufferStatus status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegated_decoder_delegate_);

  delegated_decoder_delegate_->OnPushBufferComplete(status);
}

void AudioDecoderPipelineNode::OnEndOfStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegated_decoder_delegate_);

  delegated_decoder_delegate_->OnEndOfStream();
}

void AudioDecoderPipelineNode::OnDecoderError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegated_decoder_delegate_);

  delegated_decoder_delegate_->OnDecoderError();
}

void AudioDecoderPipelineNode::OnKeyStatusChanged(const std::string& key_id,
                                                  CastKeyStatus key_status,
                                                  uint32_t system_code) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegated_decoder_delegate_);

  delegated_decoder_delegate_->OnKeyStatusChanged(key_id, key_status,
                                                  system_code);
}

void AudioDecoderPipelineNode::OnVideoResolutionChanged(const Size& size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegated_decoder_delegate_);

  delegated_decoder_delegate_->OnVideoResolutionChanged(size);
}

}  // namespace media
}  // namespace chromecast
