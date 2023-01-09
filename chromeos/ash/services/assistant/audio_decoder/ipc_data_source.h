// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_AUDIO_DECODER_IPC_DATA_SOURCE_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_AUDIO_DECODER_IPC_DATA_SOURCE_H_

#include <stdint.h>

#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "chromeos/ash/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "media/base/data_source.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::assistant {

// Provides data source to the audio stream decoder. Class must be created and
// destroyed on a same thread. The thread must not be blocked for read
// operations to succeed.
class IPCDataSource : public media::DataSource {
 public:
  // May only be called on the utility thread.
  explicit IPCDataSource(
      mojo::PendingRemote<mojom::AssistantMediaDataSource> media_data_source);

  IPCDataSource(const IPCDataSource&) = delete;
  IPCDataSource& operator=(const IPCDataSource&) = delete;

  ~IPCDataSource() override;

  // media::DataSource implementation. The methods may be called on any single
  // thread. First usage of these methods attaches a thread checker.
  void Stop() override;
  void Abort() override;
  void Read(int64_t position,
            int size,
            uint8_t* destination,
            ReadCB callback) override;
  bool GetSize(int64_t* size_out) override;
  bool IsStreaming() override;
  void SetBitrate(int bitrate) override;
  bool PassedTimingAllowOriginCheck() override;
  bool WouldTaintOrigin() override;

 private:
  // Media data read helpers: must be run on the utility thread.
  void ReadMediaData(uint8_t* destination, ReadCB callback, int size);
  void ReadDone(uint8_t* destination,
                ReadCB callback,
                uint32_t requested_size,
                const std::vector<uint8_t>& data);

  mojo::Remote<mojom::AssistantMediaDataSource> media_data_source_;

  scoped_refptr<base::SequencedTaskRunner> utility_task_runner_;

  THREAD_CHECKER(utility_thread_checker_);

  // Enforces that the DataSource methods are called on one other thread only.
  THREAD_CHECKER(data_source_thread_checker_);
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_AUDIO_DECODER_IPC_DATA_SOURCE_H_
