// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/loopback_handler.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "chromecast/media/cma/backend/mixer/mixer_loopback_connection.h"
#include "chromecast/net/io_buffer_pool.h"
#include "chromecast/public/media/external_audio_pipeline_shlib.h"

namespace chromecast {
namespace media {

class LoopbackHandler::LoopbackIO {
 public:
  LoopbackIO() = default;
  ~LoopbackIO() = default;

  void AddConnection(std::unique_ptr<MixerLoopbackConnection> connection) {
    MixerLoopbackConnection* ptr = connection.get();
    connections_[ptr] = std::move(connection);
    ptr->SetErrorCallback(base::BindOnce(&LoopbackIO::RemoveConnection,
                                         base::Unretained(this), ptr));
    if (sample_format_ != kUnknownSampleFormat) {
      ptr->SetStreamConfig(sample_format_, sample_rate_, num_channels_,
                           data_size_);
    }
  }

  void SetStreamConfig(SampleFormat sample_format,
                       int sample_rate,
                       int num_channels,
                       int data_size) {
    sample_format_ = sample_format;
    sample_rate_ = sample_rate;
    num_channels_ = num_channels;
    data_size_ = data_size;

    for (const auto& c : connections_) {
      c.second->SetStreamConfig(sample_format_, sample_rate_, num_channels_,
                                data_size_);
    }
  }

  void SendData(scoped_refptr<net::IOBuffer> audio_buffer,
                int data_size_bytes,
                int64_t timestamp) {
    for (const auto& c : connections_) {
      c.second->SendAudio(audio_buffer, data_size_bytes, timestamp);
    }
  }

 private:
  void RemoveConnection(MixerLoopbackConnection* connection) {
    connections_.erase(connection);
  }

  base::flat_map<MixerLoopbackConnection*,
                 std::unique_ptr<MixerLoopbackConnection>>
      connections_;

  SampleFormat sample_format_ = kUnknownSampleFormat;
  int sample_rate_ = 0;
  int num_channels_ = 0;
  int data_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(LoopbackIO);
};

class LoopbackHandler::ExternalLoopbackHandler
    : public ExternalAudioPipelineShlib::LoopbackAudioObserver {
 public:
  explicit ExternalLoopbackHandler(LoopbackHandler* owner) : owner_(owner) {
    DCHECK(owner_);
    ExternalAudioPipelineShlib::AddExternalLoopbackAudioObserver(this);
  }

  void Destroy() {
    {
      base::AutoLock lock(lock_);
      destroyed_ = true;
    }

    ExternalAudioPipelineShlib::RemoveExternalLoopbackAudioObserver(this);
  }

 private:
  ~ExternalLoopbackHandler() override = default;

  // ExternalAudioPipelineShlib::LoopbackAudioObserver implementation:
  void OnLoopbackAudio(int64_t timestamp,
                       SampleFormat format,
                       int sample_rate,
                       int num_channels,
                       uint8_t* data,
                       int length) override {
    base::AutoLock lock(lock_);
    if (!destroyed_) {
      owner_->SendDataInternal(timestamp, format, sample_rate, num_channels,
                               data, length);
    }
  }

  void OnLoopbackInterrupted() override {
    base::AutoLock lock(lock_);
    if (!destroyed_) {
      owner_->SendInterruptInternal();
    }
  }

  void OnRemoved() override {
    delete this;
  }

  LoopbackHandler* const owner_;

  base::Lock lock_;
  bool destroyed_ GUARDED_BY(lock_) = false;

  DISALLOW_COPY_AND_ASSIGN(ExternalLoopbackHandler);
};

void LoopbackHandler::ExternalDeleter::operator()(
    ExternalLoopbackHandler* obj) {
  obj->Destroy();
}

LoopbackHandler::LoopbackHandler(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : LoopbackHandler(std::move(io_task_runner),
                      ExternalAudioPipelineShlib::IsSupported()) {}

LoopbackHandler::LoopbackHandler(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    bool use_external_audio_pipeline)
    : io_(std::move(io_task_runner)) {
  if (use_external_audio_pipeline) {
    external_handler_.reset(new ExternalLoopbackHandler(this));
  }
}

LoopbackHandler::~LoopbackHandler() = default;

void LoopbackHandler::AddConnection(
    std::unique_ptr<MixerLoopbackConnection> connection) {
  io_.Post(FROM_HERE, &LoopbackIO::AddConnection, std::move(connection));
}

void LoopbackHandler::SetDataSize(int data_size_bytes) {
  if (external_handler_) {
    return;
  }

  if (SetDataSizeInternal(data_size_bytes) && sample_rate_ != 0) {
    io_.Post(FROM_HERE, &LoopbackIO::SetStreamConfig, format_, sample_rate_,
             num_channels_, data_size_);
  }
}

void LoopbackHandler::SendData(int64_t timestamp,
                               int sample_rate,
                               int num_channels,
                               float* data,
                               int frames) {
  if (external_handler_) {
    return;
  }

  int data_size_bytes = frames * num_channels * sizeof(float);
  SendDataInternal(timestamp, kSampleFormatF32, sample_rate, num_channels, data,
                   data_size_bytes);
}

void LoopbackHandler::SendInterrupt() {
  if (external_handler_) {
    return;
  }
  SendInterruptInternal();
}

bool LoopbackHandler::SetDataSizeInternal(int data_size_bytes) {
  if (buffer_pool_ && data_size_ >= data_size_bytes) {
    return false;
  }

  data_size_ = data_size_bytes;
  buffer_pool_ = base::MakeRefCounted<IOBufferPool>(
      data_size_ + mixer_service::MixerSocket::kAudioMessageHeaderSize,
      std::numeric_limits<size_t>::max(), true /* threadsafe */);
  buffer_pool_->Preallocate(4);
  return true;
}

void LoopbackHandler::SendDataInternal(int64_t timestamp,
                                       SampleFormat format,
                                       int sample_rate,
                                       int num_channels,
                                       void* data,
                                       int data_size_bytes) {
  bool data_size_changed = SetDataSizeInternal(data_size_bytes);
  if (format != format_ || sample_rate != sample_rate_ ||
      num_channels != num_channels_ || data_size_changed) {
    format_ = format;
    sample_rate_ = sample_rate;
    num_channels_ = num_channels;
    io_.Post(FROM_HERE, &LoopbackIO::SetStreamConfig, format_, sample_rate_,
             num_channels_, data_size_);
  }

  DCHECK_LE(data_size_bytes, data_size_);
  DCHECK(buffer_pool_);
  auto buffer = buffer_pool_->GetBuffer();
  memcpy(buffer->data() + mixer_service::MixerSocket::kAudioMessageHeaderSize,
         data, data_size_bytes);
  io_.Post(FROM_HERE, &LoopbackIO::SendData, std::move(buffer), data_size_bytes,
           timestamp);
}

void LoopbackHandler::SendInterruptInternal() {
  if (!buffer_pool_) {
    return;
  }

  io_.Post(FROM_HERE, &LoopbackIO::SendData, buffer_pool_->GetBuffer(), 0, 0);
}

}  // namespace media
}  // namespace chromecast
