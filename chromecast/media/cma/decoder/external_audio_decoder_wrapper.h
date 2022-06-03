// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_DECODER_EXTERNAL_AUDIO_DECODER_WRAPPER_H_
#define CHROMECAST_MEDIA_CMA_DECODER_EXTERNAL_AUDIO_DECODER_WRAPPER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/media/api/cast_audio_decoder.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/external_audio_decoder.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace media {
class AudioBus;
}

namespace chromecast {
namespace media {
class DecoderBufferBase;

// Wrapper API for external (3P) decoder library.
class ExternalAudioDecoderWrapper : public ExternalAudioDecoder::Delegate,
                                    public CastAudioDecoder {
 public:
  static bool IsSupportedConfig(const AudioConfig& config);

  ExternalAudioDecoderWrapper(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const AudioConfig& config,
      CastAudioDecoder::OutputFormat output_format);
  ExternalAudioDecoderWrapper(const ExternalAudioDecoderWrapper&) = delete;
  ExternalAudioDecoderWrapper& operator=(const ExternalAudioDecoderWrapper&) =
      delete;
  ~ExternalAudioDecoderWrapper() override;

  bool initialized() const { return decoder_ != nullptr; }

 private:
  class DecodedBuffer;

  // CastAudioDecoder implementation:
  const AudioConfig& GetOutputConfig() const override;
  void Decode(scoped_refptr<media::DecoderBufferBase> data,
              DecodeCallback decode_callback) override;

  void DecodeDeferred(scoped_refptr<media::DecoderBufferBase> data,
                      DecodeCallback decode_callback);
  void ConvertToS16(DecodedBuffer* buffer);

  // ExternalAudioDecoder::Delegate implementation:
  void* AllocateBuffer(size_t bytes) override;
  void OnDecodedBuffer(size_t decoded_size_bytes,
                       const AudioConfig& config) override;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const CastAudioDecoder::OutputFormat output_format_;
  ExternalAudioDecoder* const decoder_;

  AudioConfig output_config_;
  std::vector<scoped_refptr<DecodedBuffer>> buffers_;
  bool pending_buffer_ = false;

  std::unique_ptr<::media::AudioBus> conversion_buffer_;

  base::WeakPtrFactory<ExternalAudioDecoderWrapper> weak_factory_{this};
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_DECODER_EXTERNAL_AUDIO_DECODER_WRAPPER_H_
