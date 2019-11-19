// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/audio_decoder/ipc_data_source.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/message.h"

namespace chromeos {
namespace assistant {

IPCDataSource::IPCDataSource(
    mojo::PendingRemote<mojom::AssistantMediaDataSource> media_data_source)
    : media_data_source_(std::move(media_data_source)),
      utility_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DETACH_FROM_THREAD(data_source_thread_checker_);
}

IPCDataSource::~IPCDataSource() {
  DCHECK_CALLED_ON_VALID_THREAD(utility_thread_checker_);
}

void IPCDataSource::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);
}

void IPCDataSource::Abort() {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);
}

void IPCDataSource::Read(int64_t position,
                         int size,
                         uint8_t* destination,
                         const DataSource::ReadCB& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);

  utility_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&IPCDataSource::ReadMediaData, base::Unretained(this),
                     destination, callback, size));
}

bool IPCDataSource::GetSize(int64_t* size_out) {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);
  *size_out = 0;
  return false;
}

bool IPCDataSource::IsStreaming() {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);
  return true;
}

void IPCDataSource::SetBitrate(int bitrate) {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);
}

void IPCDataSource::ReadMediaData(uint8_t* destination,
                                  const DataSource::ReadCB& callback,
                                  int size) {
  DCHECK_CALLED_ON_VALID_THREAD(utility_thread_checker_);
  CHECK_GE(size, 0);

  media_data_source_->Read(
      size, base::BindOnce(&IPCDataSource::ReadDone, base::Unretained(this),
                           destination, callback, size));
}

void IPCDataSource::ReadDone(uint8_t* destination,
                             const DataSource::ReadCB& callback,
                             uint32_t requested_size,
                             const std::vector<uint8_t>& data) {
  DCHECK_CALLED_ON_VALID_THREAD(utility_thread_checker_);
  if (data.size() > requested_size) {
    mojo::ReportBadMessage("IPCDataSource::ReadDone: Unexpected data size.");
    callback.Run(0);
    return;
  }

  std::copy(data.begin(), data.end(), destination);
  callback.Run(data.size());
}

}  // namespace assistant
}  // namespace chromeos
