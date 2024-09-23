// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/api/cast_audio_decoder.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/media/cma/decoder/external_audio_decoder_wrapper.h"
#include "chromecast/media/common/base/decoder_config_logging.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/cdm_context.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/base/status.h"
#include "media/filters/ffmpeg_audio_decoder.h"

namespace chromecast {
namespace media {

namespace {

// This class wraps the underlying data of a DecoderBufferBase.
// This class does not take the ownership of the data. The DecoderBufferBase
// is still responsible for deleting the data. This class holds a reference
// to the DecoderBufferBase so that it lives longer than this DecoderBuffer.
class DecoderBufferExternalMemory
    : public ::media::DecoderBuffer::ExternalMemory {
 public:
  explicit DecoderBufferExternalMemory(scoped_refptr<DecoderBufferBase> buffer)
      : buffer_(std::move(buffer)) {}

  const base::span<const uint8_t> Span() const override {
    return {buffer_->data(), buffer_->data_size()};
  }

 private:
  scoped_refptr<DecoderBufferBase> buffer_;
};

class CastAudioDecoderImpl : public CastAudioDecoder {
 public:
  CastAudioDecoderImpl(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       const media::AudioConfig& config,
                       OutputFormat output_format)
      : task_runner_(std::move(task_runner)),
        output_format_(output_format),
        weak_factory_(this) {
    weak_this_ = weak_factory_.GetWeakPtr();
    DCHECK(task_runner_);

    input_config_ = config;
    input_config_.encryption_scheme = EncryptionScheme::kUnencrypted;

    output_config_ = input_config_;
    output_config_.codec = kCodecPCM;
    output_config_.sample_format =
        (output_format_ == kOutputSigned16 ? kSampleFormatS16
                                           : kSampleFormatPlanarF32);

    decoder_ = std::make_unique<::media::FFmpegAudioDecoder>(task_runner_,
                                                             &media_log_);
    decoder_->Initialize(
        media::DecoderConfigAdapter::ToMediaAudioDecoderConfig(input_config_),
        nullptr,
        base::BindRepeating(&CastAudioDecoderImpl::OnInitialized, weak_this_),
        base::BindRepeating(&CastAudioDecoderImpl::OnDecoderOutput, weak_this_),
        base::NullCallback());
    // Unfortunately there is no result from decoder_->Initialize() until later
    // (the pipeline status callback is posted to the task runner).
  }

  CastAudioDecoderImpl(const CastAudioDecoderImpl&) = delete;
  CastAudioDecoderImpl& operator=(const CastAudioDecoderImpl&) = delete;

  // CastAudioDecoder implementation:
  const AudioConfig& GetOutputConfig() const override { return output_config_; }

  void Decode(scoped_refptr<media::DecoderBufferBase> data,
              DecodeCallback decode_callback) override {
    DCHECK(decode_callback);
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    if (data->decrypt_context() != nullptr || error_) {
      if (data->decrypt_context() != nullptr) {
        LOG(ERROR) << "Audio decoder doesn't support encrypted stream";
      }

      // Post the task to ensure that |decode_callback| is not called from
      // within a call to Decode().
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CastAudioDecoderImpl::CallDecodeCallback,
                                    weak_this_, std::move(decode_callback),
                                    kDecodeError, std::move(data)));
    } else if (!initialized_ || decode_pending_) {
      decode_queue_.push(
          std::make_pair(std::move(data), std::move(decode_callback)));
    } else {
      DecodeNow(std::move(data), std::move(decode_callback));
    }
  }

 private:
  typedef std::pair<scoped_refptr<media::DecoderBufferBase>, DecodeCallback>
      DecodeBufferCallbackPair;

  void CallDecodeCallback(DecodeCallback decode_callback,
                          Status status,
                          scoped_refptr<media::DecoderBufferBase> data) {
    std::move(decode_callback).Run(status, output_config_, std::move(data));
  }

  void DecodeNow(scoped_refptr<media::DecoderBufferBase> data,
                 DecodeCallback decode_callback) {
    if (data->end_of_stream()) {
      // Post the task to ensure that |decode_callback| is not called from
      // within a call to Decode().
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CastAudioDecoderImpl::CallDecodeCallback,
                                    weak_this_, std::move(decode_callback),
                                    kDecodeOk, std::move(data)));
      return;
    }

    // FFmpegAudioDecoder requires a timestamp to be set.
    base::TimeDelta timestamp = base::Microseconds(data->timestamp());
    if (timestamp == ::media::kNoTimestamp) {
      timestamp = base::TimeDelta();
      data->set_timestamp(timestamp);
    }

    decode_pending_ = true;
    pending_decode_callback_ = std::move(decode_callback);

    auto media_buffer = ::media::DecoderBuffer::FromExternalMemory(
        std::make_unique<DecoderBufferExternalMemory>(std::move(data)));
    media_buffer->set_timestamp(timestamp);

    decoder_->Decode(std::move(media_buffer),
                     base::BindRepeating(&CastAudioDecoderImpl::OnDecodeStatus,
                                         weak_this_, timestamp));
  }

  void OnInitialized(::media::DecoderStatus status) {
    DCHECK(!initialized_);
    initialized_ = true;
    if (status.is_ok()) {
      if (!decode_queue_.empty()) {
        auto& d = decode_queue_.front();
        DecodeNow(std::move(d.first), std::move(d.second));
        decode_queue_.pop();
      }
      return;
    }

    error_ = true;
    LOG(ERROR) << "Failed to initialize audio decoder";
    LOG(INFO) << "Config:";
    LOG(INFO) << "\tCodec: " << input_config_.codec;
    LOG(INFO) << "\tSample format: " << input_config_.sample_format;
    LOG(INFO) << "\tChannels: " << input_config_.channel_number;
    LOG(INFO) << "\tSample rate: " << input_config_.samples_per_second;

    while (!decode_queue_.empty()) {
      auto& d = decode_queue_.front();
      std::move(d.second).Run(kDecodeError, output_config_, std::move(d.first));
      decode_queue_.pop();
    }
  }

  void OnDecodeStatus(base::TimeDelta buffer_timestamp,
                      ::media::DecoderStatus status) {
    DCHECK(pending_decode_callback_);

    Status result_status = kDecodeOk;
    scoped_refptr<media::DecoderBufferBase> decoded;
    if (status.is_ok() && !decoded_chunks_.empty()) {
      decoded = ConvertDecoded();
    } else {
      if (!status.is_ok())
        result_status = kDecodeError;
      decoded = base::MakeRefCounted<media::DecoderBufferAdapter>(
          output_config_.id, base::MakeRefCounted<::media::DecoderBuffer>(0));
    }
    decoded_chunks_.clear();
    decoded->set_timestamp(buffer_timestamp);
    base::WeakPtr<CastAudioDecoderImpl> self = weak_factory_.GetWeakPtr();
    std::move(pending_decode_callback_)
        .Run(result_status, output_config_, std::move(decoded));
    if (!self)
      return;  // Return immediately if the decode callback deleted this.

    // Do not reset decode_pending_ to false until after the callback has
    // finished running because the callback may call Decode().
    decode_pending_ = false;

    if (decode_queue_.empty())
      return;

    auto& d = decode_queue_.front();
    // Calling DecodeNow() here does not result in a loop, because
    // OnDecodeStatus() is always called asynchronously (guaranteed by the
    // AudioDecoder interface).
    DecodeNow(std::move(d.first), std::move(d.second));
    decode_queue_.pop();
  }

  void OnDecoderOutput(scoped_refptr<::media::AudioBuffer> decoded) {
    if (decoded->sample_rate() != output_config_.samples_per_second) {
      LOG(WARNING) << "sample_rate changed to " << decoded->sample_rate()
                   << " from " << output_config_.samples_per_second;
      output_config_.samples_per_second = decoded->sample_rate();
    }

    ChannelLayout decoded_channel_layout =
        DecoderConfigAdapter::ToChannelLayout(decoded->channel_layout());
    if (decoded->channel_count() != output_config_.channel_number ||
        decoded_channel_layout != output_config_.channel_layout) {
      LOG(WARNING) << "channel_count changed to " << decoded->channel_count()
                   << " from " << output_config_.channel_number
                   << ", channel_layout changed to "
                   << static_cast<int>(decoded_channel_layout) << " from "
                   << static_cast<int>(output_config_.channel_layout);
      output_config_.channel_number = decoded->channel_count();
      output_config_.channel_layout =
          DecoderConfigAdapter::ToChannelLayout(decoded->channel_layout());
      decoded_bus_.reset();
    }

    decoded_chunks_.push_back(std::move(decoded));
  }

  scoped_refptr<media::DecoderBufferBase> ConvertDecoded() {
    DCHECK(!decoded_chunks_.empty());
    int num_frames = 0;
    for (auto& chunk : decoded_chunks_)
      num_frames += chunk->frame_count();

    // Copy decoded data into an AudioBus for conversion.
    if (!decoded_bus_ || decoded_bus_->frames() < num_frames) {
      decoded_bus_ = ::media::AudioBus::Create(output_config_.channel_number,
                                               num_frames * 2);
    }
    int bus_frame_offset = 0;
    for (auto& chunk : decoded_chunks_) {
      chunk->ReadFrames(chunk->frame_count(), 0, bus_frame_offset,
                        decoded_bus_.get());
      bus_frame_offset += chunk->frame_count();
    }

    return FinishConversion(decoded_bus_.get(), bus_frame_offset);
  }

  scoped_refptr<media::DecoderBufferBase> FinishConversion(
      ::media::AudioBus* bus,
      int num_frames) {
    int size =
        num_frames * bus->channels() * OutputFormatSizeInBytes(output_format_);
    auto result = base::MakeRefCounted<::media::DecoderBuffer>(size);

    if (output_format_ == kOutputSigned16) {
      bus->ToInterleaved<::media::SignedInt16SampleTypeTraits>(
          num_frames, reinterpret_cast<int16_t*>(result->writable_data()));
    } else if (output_format_ == kOutputPlanarFloat) {
      // Data in an AudioBus is already in planar float format; just copy each
      // channel into the result buffer in order.
      float* ptr = reinterpret_cast<float*>(result->writable_data());
      for (int c = 0; c < bus->channels(); ++c) {
        std::copy_n(bus->channel(c), num_frames, ptr);
        ptr += num_frames;
      }
    } else {
      NOTREACHED();
    }

    result->set_duration(
        base::Microseconds(num_frames * base::Time::kMicrosecondsPerSecond /
                           output_config_.samples_per_second));
    return base::MakeRefCounted<media::DecoderBufferAdapter>(output_config_.id,
                                                             result);
  }

  ::media::NullMediaLog media_log_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  OutputFormat output_format_;
  bool initialized_ = false;
  bool error_ = false;
  media::AudioConfig input_config_;
  media::AudioConfig output_config_;

  std::unique_ptr<::media::AudioDecoder> decoder_;
  base::queue<DecodeBufferCallbackPair> decode_queue_;

  bool decode_pending_ = false;
  DecodeCallback pending_decode_callback_;
  std::vector<scoped_refptr<::media::AudioBuffer>> decoded_chunks_;

  std::unique_ptr<::media::AudioBus> decoded_bus_;

  base::WeakPtr<CastAudioDecoderImpl> weak_this_;
  base::WeakPtrFactory<CastAudioDecoderImpl> weak_factory_;
};

}  // namespace

// static
std::unique_ptr<CastAudioDecoder> CastAudioDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const media::AudioConfig& config,
    OutputFormat output_format) {
  if (ExternalAudioDecoderWrapper::IsSupportedConfig(config)) {
    auto external_decoder = std::make_unique<ExternalAudioDecoderWrapper>(
        std::move(task_runner), config, output_format);
    if (!external_decoder->initialized()) {
      return nullptr;
    }
    return external_decoder;
  }

  return std::make_unique<CastAudioDecoderImpl>(std::move(task_runner), config,
                                                output_format);
}

// static
int CastAudioDecoder::OutputFormatSizeInBytes(
    CastAudioDecoder::OutputFormat format) {
  switch (format) {
    case CastAudioDecoder::OutputFormat::kOutputSigned16:
      return 2;
    case CastAudioDecoder::OutputFormat::kOutputPlanarFloat:
      return 4;
  }
  NOTREACHED();
}

}  // namespace media
}  // namespace chromecast
