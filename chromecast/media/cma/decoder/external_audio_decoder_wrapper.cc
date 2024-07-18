// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/decoder/external_audio_decoder_wrapper.h"

#include <algorithm>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {

namespace {

const char kDefaultExternalDecoderPath[] = "libcast_external_decoder.so";

const char kSupportedConfigFunction[] =
    "ExternalAudioDecoder_IsSupportedConfig";
const char kCreateFunction[] = "ExternalAudioDecoder_CreateDecoder";
const char kDeleteFunction[] = "ExternalAudioDecoder_DeleteDecoder";

const size_t kMinConversionBufferSize = 256;

class ExternalDecoderLib {
 public:
  ExternalDecoderLib() : lib_(base::FilePath(kDefaultExternalDecoderPath)) {
    if (lib_.is_valid()) {
      supported_config_func_ = reinterpret_cast<IsSupportedConfigFunction>(
          lib_.GetFunctionPointer(kSupportedConfigFunction));
      create_func_ = reinterpret_cast<CreateFunction>(
          lib_.GetFunctionPointer(kCreateFunction));
      delete_func_ = reinterpret_cast<DeleteFunction>(
          lib_.GetFunctionPointer(kDeleteFunction));

      LOG_IF(ERROR, !supported_config_func_)
          << "Missing function: " << kSupportedConfigFunction;
      LOG_IF(ERROR, !create_func_) << "Missing function: " << kCreateFunction;
      LOG_IF(ERROR, !delete_func_) << "Missing function: " << kDeleteFunction;
    }
  }

  ExternalDecoderLib(const ExternalDecoderLib&) = delete;
  ExternalDecoderLib& operator=(const ExternalDecoderLib&) = delete;

  ~ExternalDecoderLib() = default;

  bool IsSupportedConfig(const AudioConfig& config) {
    if (!supported_config_func_ || !create_func_ || !delete_func_) {
      return false;
    }

    return supported_config_func_(config);
  }

  ExternalAudioDecoder* CreateDecoder(
      ExternalAudioDecoder::Delegate* delegate,
      const chromecast::media::AudioConfig& config) {
    if (!create_func_ || !delete_func_) {
      return nullptr;
    }

    return create_func_(delegate, config);
  }

  void DeleteDecoder(ExternalAudioDecoder* decoder) {
    DCHECK(delete_func_);
    delete_func_(decoder);
  }

 private:
  using IsSupportedConfigFunction =
      decltype(&ExternalAudioDecoder_IsSupportedConfig);
  using CreateFunction = decltype(&ExternalAudioDecoder_CreateDecoder);
  using DeleteFunction = decltype(&ExternalAudioDecoder_DeleteDecoder);

  base::ScopedNativeLibrary lib_;
  IsSupportedConfigFunction supported_config_func_ = nullptr;
  CreateFunction create_func_ = nullptr;
  DeleteFunction delete_func_ = nullptr;
};

ExternalDecoderLib& GetLib() {
  static base::NoDestructor<ExternalDecoderLib> g_lib;
  return *g_lib;
}

AudioConfig BuildOutputConfig(const AudioConfig& input_config,
                              CastAudioDecoder::OutputFormat output_format,
                              ExternalAudioDecoder* decoder) {
  AudioConfig output_config = input_config;
  output_config.encryption_scheme = EncryptionScheme::kUnencrypted;
  output_config.codec = kCodecPCM;
  output_config.sample_format =
      (output_format == CastAudioDecoder::kOutputSigned16
           ? kSampleFormatS16
           : kSampleFormatPlanarF32);
  output_config.channel_number = decoder->GetNumOutputChannels();
  if (output_config.channel_number <= 0) {
    output_config.channel_number = input_config.channel_number;
  }
  return output_config;
}

}  // namespace

class ExternalAudioDecoderWrapper::DecodedBuffer : public DecoderBufferBase {
 public:
  DecodedBuffer(StreamId stream_id, size_t capacity)
      : stream_id_(stream_id),
        capacity_(capacity),
        data_(std::make_unique<uint8_t[]>(capacity_)) {}

  void set_size(size_t size) {
    DCHECK_LE(size, capacity_);
    size_ = size;
  }

  // DecoderBufferBase implementation:
  StreamId stream_id() const override { return stream_id_; }
  int64_t timestamp() const override { return timestamp_.InMicroseconds(); }
  void set_timestamp(base::TimeDelta timestamp) override {
    timestamp_ = timestamp;
  }
  const uint8_t* data() const override { return data_.get(); }
  uint8_t* writable_data() const override { return data_.get(); }
  size_t data_size() const override { return size_; }
  const CastDecryptConfig* decrypt_config() const override { return nullptr; }
  bool end_of_stream() const override { return false; }
  bool is_key_frame() const override { return false; }

 private:
  ~DecodedBuffer() override = default;

  const StreamId stream_id_;
  const size_t capacity_;

  const std::unique_ptr<uint8_t[]> data_;

  base::TimeDelta timestamp_;
  size_t size_ = 0;
};

// static
bool ExternalAudioDecoderWrapper::IsSupportedConfig(const AudioConfig& config) {
  return GetLib().IsSupportedConfig(config);
}

ExternalAudioDecoderWrapper::ExternalAudioDecoderWrapper(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const AudioConfig& config,
    CastAudioDecoder::OutputFormat output_format)
    : task_runner_(std::move(task_runner)),
      output_format_(output_format),
      decoder_(GetLib().CreateDecoder(this, config)),
      output_config_(BuildOutputConfig(config, output_format_, decoder_)) {}

ExternalAudioDecoderWrapper::~ExternalAudioDecoderWrapper() {
  if (decoder_) {
    GetLib().DeleteDecoder(decoder_);
  }
}

const AudioConfig& ExternalAudioDecoderWrapper::GetOutputConfig() const {
  return output_config_;
}

void ExternalAudioDecoderWrapper::Decode(
    scoped_refptr<media::DecoderBufferBase> data,
    DecodeCallback decode_callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Post a task, since some callers don't expect a synchronous callback.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ExternalAudioDecoderWrapper::DecodeDeferred,
                                weak_factory_.GetWeakPtr(), std::move(data),
                                std::move(decode_callback)));
}

void ExternalAudioDecoderWrapper::DecodeDeferred(
    scoped_refptr<media::DecoderBufferBase> data,
    DecodeCallback decode_callback) {
  if (data->end_of_stream()) {
    std::move(decode_callback).Run(kDecodeOk, output_config_, std::move(data));
    return;
  }

  if (!decoder_ || !decoder_->Decode(*data)) {
    std::move(decode_callback).Run(kDecodeError, output_config_, nullptr);
    return;
  }

  size_t buffer_count = buffers_.size() - pending_buffer_;
  scoped_refptr<DecodedBuffer> decoded;
  if (buffer_count == 0) {
    decoded = base::MakeRefCounted<DecodedBuffer>(output_config_.id, 0);
  } else if (buffer_count == 1) {
    decoded = std::move(buffers_.front());
  } else {
    size_t size = 0;
    for (size_t i = 0; i < buffer_count; ++i) {
      size += buffers_[i]->data_size();
    }

    decoded = base::MakeRefCounted<DecodedBuffer>(output_config_.id, size);
    decoded->set_size(size);

    const size_t frame_size = sizeof(float) * output_config_.channel_number;
    size_t total_frames = size / frame_size;
    for (int c = 0; c < output_config_.channel_number; ++c) {
      uint8_t* dest =
          decoded->writable_data() + c * total_frames * sizeof(float);
      for (size_t i = 0; i < buffer_count; ++i) {
        size_t frames = buffers_[i]->data_size() / frame_size;
        void* src = buffers_[i]->writable_data() + c * frames * sizeof(float);
        memcpy(dest, src, frames * sizeof(float));
        dest += frames * sizeof(float);
      }
    }
  }

  buffers_.erase(buffers_.begin(), buffers_.begin() + buffer_count);

  if (output_format_ == CastAudioDecoder::kOutputSigned16) {
    ConvertToS16(decoded.get());
  }

  decoded->set_timestamp(base::Microseconds(data->timestamp()));
  std::move(decode_callback).Run(kDecodeOk, output_config_, std::move(decoded));
}

void ExternalAudioDecoderWrapper::ConvertToS16(DecodedBuffer* buffer) {
  const int channels = output_config_.channel_number;
  const size_t frame_size = sizeof(float) * channels;
  const size_t frames = buffer->data_size() / frame_size;

  if (!conversion_buffer_ ||
      conversion_buffer_->frames() < static_cast<int>(frames) ||
      conversion_buffer_->channels() != channels) {
    conversion_buffer_ = ::media::AudioBus::Create(
        channels, std::max(frames * 2, kMinConversionBufferSize));
  }

  const float* src = reinterpret_cast<const float*>(buffer->data());
  for (int c = 0; c < channels; ++c) {
    std::copy_n(src + c * frames, frames, conversion_buffer_->channel(c));
  }

  int16_t* dest = reinterpret_cast<int16_t*>(buffer->writable_data());
  conversion_buffer_
      ->ToInterleavedPartial<::media::SignedInt16SampleTypeTraits>(0, frames,
                                                                   dest);

  buffer->set_size(frames * channels * sizeof(int16_t));
}

void* ExternalAudioDecoderWrapper::AllocateBuffer(size_t bytes) {
  auto buffer = base::MakeRefCounted<DecodedBuffer>(output_config_.id, bytes);
  void* ptr = buffer->writable_data();
  buffers_.push_back(std::move(buffer));
  pending_buffer_ = true;
  return ptr;
}

void ExternalAudioDecoderWrapper::OnDecodedBuffer(size_t decoded_size_bytes,
                                                  const AudioConfig& config) {
  DCHECK(!buffers_.empty());
  size_t frame_size = sizeof(float) * config.channel_number;
  DCHECK_EQ(decoded_size_bytes % frame_size, 0u);

  buffers_.back()->set_size(decoded_size_bytes);
  output_config_.channel_number = config.channel_number;
  output_config_.samples_per_second = config.samples_per_second;
  pending_buffer_ = false;
}

}  // namespace media
}  // namespace chromecast
