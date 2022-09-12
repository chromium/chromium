// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_LOOPBACK_HANDLER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_LOOPBACK_HANDLER_H_

#include <cstdint>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "chromecast/public/media/decoder_config.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace chromecast {
class IOBufferPool;

namespace media {
enum class LoopbackInterruptReason;
class MixerLoopbackConnection;

// Handles loopback audio from the mixer or external audio pipeline.
class LoopbackHandler {
 public:
  explicit LoopbackHandler(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  LoopbackHandler(scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                  bool use_external_audio_pipeline);

  LoopbackHandler(const LoopbackHandler&) = delete;
  LoopbackHandler& operator=(const LoopbackHandler&) = delete;

  ~LoopbackHandler();

  // Adds a new loopback connection.
  void AddConnection(std::unique_ptr<MixerLoopbackConnection> connection);

  // Sets the expected size of audio data (in bytes) of audio data from the
  // mixer.
  void SetDataSize(int data_size_bytes);

  // Passes loopback data from the mixer to any connected observers.
  void SendData(int64_t timestamp,
                int sample_rate,
                int num_channels,
                float* data,
                int frames);

  // Sends a 'loopback interrupted' signal to any connected observers.
  void SendInterrupt(LoopbackInterruptReason reason);

 private:
  class ExternalLoopbackHandler;
  struct ExternalDeleter {
    void operator()(ExternalLoopbackHandler* obj);
  };
  class LoopbackIO;

  bool SetDataSizeInternal(int data_size_bytes);
  void SendDataInternal(int64_t timestamp,
                        SampleFormat format,
                        int sample_rate,
                        int num_channels,
                        void* data,
                        int data_size_bytes);
  void SendInterruptInternal(LoopbackInterruptReason reason);

  scoped_refptr<IOBufferPool> buffer_pool_;

  int data_size_ = 0;
  SampleFormat format_ = kUnknownSampleFormat;
  int sample_rate_ = 0;
  int num_channels_ = 0;

  base::SequenceBound<LoopbackIO> io_;
  std::unique_ptr<ExternalLoopbackHandler, ExternalDeleter> external_handler_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_LOOPBACK_HANDLER_H_
