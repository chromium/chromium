// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/decoder/cast_audio_decoder.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/queue.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/media/cma/base/decoder_config_logging.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/cdm_context.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/filters/ffmpeg_audio_decoder.h"

namespace chromecast {
namespace media {

namespace {

class CastAudioDecoderImpl : public CastAudioDecoder {
 public:
  CastAudioDecoderImpl(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       InitializedCallback initialized_callback,
                       OutputFormat output_format)
      : task_runner_(std::move(task_runner)),
        initialized_callback_(std::move(initialized_callback)),
        output_format_(output_format),
        initialized_(false),
        decode_pending_(false),
        weak_factory_(this) {
    weak_this_ = weak_factory_.GetWeakPtr();
    DCHECK(task_runner_);
    DCHECK(initialized_callback_);
  }

  void Initialize(const media::AudioConfig& config) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!initialized_);

    config_ = config;
    if (config_.is_encrypted()) {
      LOG(ERROR) << "Cannot decode encrypted audio";
      // TODO(kmackay) Should call OnInitialized(false) here, but that generally
      // causes the browsertests to crash since it happens during the render
      // pipeline initialization.
      config_.encryption_scheme = EncryptionScheme::kUnencrypted;
    }

    decoder_ = std::make_unique<::media::FFmpegAudioDecoder>(task_runner_,
                                                             &media_log_);
    decoder_->Initialize(
        media::DecoderConfigAdapter::ToMediaAudioDecoderConfig(config_),
        nullptr,
        base::BindRepeating(&CastAudioDecoderImpl::OnInitialized, weak_this_),
        base::BindRepeating(&CastAudioDecoderImpl::OnDecoderOutput, weak_this_),
        base::NullCallback());
    // Unfortunately there is no result from decoder_->Initialize() until later
    // (the pipeline status callback is posted to the task runner).
  }

  // CastAudioDecoder implementation:
  void Decode(scoped_refptr<media::DecoderBufferBase> data,
              DecodeCallback decode_callback) override {
    DCHECK(decode_callback);
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    if (data->decrypt_context() != nullptr) {
      LOG(ERROR) << "Audio decoder doesn't support encrypted stream";
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
    std::move(decode_callback).Run(status, config_, std::move(data));
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
    base::TimeDelta timestamp =
        base::TimeDelta::FromMicroseconds(data->timestamp());
    if (timestamp == ::media::kNoTimestamp)
      data->set_timestamp(base::TimeDelta());

    decode_pending_ = true;
    pending_decode_callback_ = std::move(decode_callback);
    decoder_->Decode(data->ToMediaBuffer(),
                     base::BindRepeating(&CastAudioDecoderImpl::OnDecodeStatus,
                                         weak_this_, timestamp));
  }

  void OnInitialized(bool success) {
    DCHECK(!initialized_);
    DCHECK(initialized_callback_);
    if (success) {
      initialized_ = true;
      if (!decode_queue_.empty()) {
        auto& d = decode_queue_.front();
        DecodeNow(std::move(d.first), std::move(d.second));
        decode_queue_.pop();
      }
    } else {
      LOG(ERROR) << "Failed to initialize FFmpegAudioDecoder";
      LOG(INFO) << "Config:";
      LOG(INFO) << "\tEncrypted: "
                << (config_.is_encrypted() ? "true" : "false");
      LOG(INFO) << "\tCodec: " << config_.codec;
      LOG(INFO) << "\tSample format: " << config_.sample_format;
      LOG(INFO) << "\tChannels: " << config_.channel_number;
      LOG(INFO) << "\tSample rate: " << config_.samples_per_second;
    }

    std::move(initialized_callback_).Run(initialized_);
  }

  void OnDecodeStatus(base::TimeDelta buffer_timestamp,
                      ::media::DecodeStatus status) {
    DCHECK(pending_decode_callback_);

    Status result_status = kDecodeOk;
    scoped_refptr<media::DecoderBufferBase> decoded;
    if (status == ::media::DecodeStatus::OK && !decoded_chunks_.empty()) {
      decoded = ConvertDecoded();
    } else {
      if (status != ::media::DecodeStatus::OK)
        result_status = kDecodeError;
      decoded = base::MakeRefCounted<media::DecoderBufferAdapter>(
          config_.id, base::MakeRefCounted<::media::DecoderBuffer>(0));
    }
    decoded_chunks_.clear();
    decoded->set_timestamp(buffer_timestamp);
    base::WeakPtr<CastAudioDecoderImpl> self = weak_factory_.GetWeakPtr();
    std::move(pending_decode_callback_)
        .Run(result_status, config_, std::move(decoded));
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
    if (decoded->sample_rate() != config_.samples_per_second) {
      LOG(WARNING) << "sample_rate changed to " << decoded->sample_rate()
                   << " from " << config_.samples_per_second;
      config_.samples_per_second = decoded->sample_rate();
    }

    if (decoded->channel_count() != config_.channel_number) {
      LOG(WARNING) << "channel_count changed to " << decoded->channel_count()
                   << " from " << config_.channel_number;
      config_.channel_number = decoded->channel_count();
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
      decoded_bus_ =
          ::media::AudioBus::Create(config_.channel_number, num_frames * 2);
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
      bus->ToInterleaved(num_frames, OutputFormatSizeInBytes(output_format_),
                         result->writable_data());
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

    result->set_duration(base::TimeDelta::FromMicroseconds(
        num_frames * base::Time::kMicrosecondsPerSecond /
        config_.samples_per_second));
    return base::MakeRefCounted<media::DecoderBufferAdapter>(config_.id,
                                                             result);
  }

  ::media::NullMediaLog media_log_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  InitializedCallback initialized_callback_;
  OutputFormat output_format_;
  bool initialized_;
  media::AudioConfig config_;

  std::unique_ptr<::media::AudioDecoder> decoder_;
  base::queue<DecodeBufferCallbackPair> decode_queue_;

  bool decode_pending_;
  DecodeCallback pending_decode_callback_;
  std::vector<scoped_refptr<::media::AudioBuffer>> decoded_chunks_;

  std::unique_ptr<::media::AudioBus> decoded_bus_;

  base::WeakPtr<CastAudioDecoderImpl> weak_this_;
  base::WeakPtrFactory<CastAudioDecoderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioDecoderImpl);
};

}  // namespace

// static
std::unique_ptr<CastAudioDecoder> CastAudioDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const media::AudioConfig& config,
    OutputFormat output_format,
    InitializedCallback initialized_callback) {
  std::unique_ptr<CastAudioDecoderImpl> decoder(new CastAudioDecoderImpl(
      std::move(task_runner), std::move(initialized_callback), output_format));
  decoder->Initialize(config);
  return std::move(decoder);
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
  return 1;
}

}  // namespace media
}  // namespace chromecast
