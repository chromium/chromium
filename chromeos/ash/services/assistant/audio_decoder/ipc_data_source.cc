// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/audio_decoder/ipc_data_source.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/message.h"

namespace ash::assistant {

IPCDataSource::IPCDataSource(
    mojo::PendingRemote<mojom::AssistantMediaDataSource> media_data_source)
    : media_data_source_(std::move(media_data_source)),
      utility_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
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
                         DataSource::ReadCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);

  utility_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&IPCDataSource::ReadMediaData, base::Unretained(this),
                     destination, std::move(callback), size));
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

bool IPCDataSource::PassedTimingAllowOriginCheck() {
  // The mojo ipc channel doesn't support this yet, so cautiously return false,
  // for now.
  // TODO(crbug.com/40243452): Rework this method to be asynchronous, if
  // possible, so that the mojo interface can be queried.
  return false;
}

bool IPCDataSource::WouldTaintOrigin() {
  // The mojo ipc channel doesn't support this yet, so cautiously return true,
  // for now.
  // TODO(crbug.com/40243452): Rework this method to be asynchronous, if
  // possible, so that the mojo interface can be queried.
  return true;
}

void IPCDataSource::ReadMediaData(uint8_t* destination,
                                  DataSource::ReadCB callback,
                                  int size) {
  DCHECK_CALLED_ON_VALID_THREAD(utility_thread_checker_);
  CHECK_GE(size, 0);

  media_data_source_->Read(
      size, base::BindOnce(&IPCDataSource::ReadDone, base::Unretained(this),
                           destination, std::move(callback), size));
}

void IPCDataSource::ReadDone(uint8_t* destination,
                             DataSource::ReadCB callback,
                             uint32_t requested_size,
                             const std::vector<uint8_t>& data) {
  DCHECK_CALLED_ON_VALID_THREAD(utility_thread_checker_);
  if (data.size() > requested_size) {
    mojo::ReportBadMessage("IPCDataSource::ReadDone: Unexpected data size.");
    std::move(callback).Run(0);
    return;
  }

  base::ranges::copy(data, destination);
  std::move(callback).Run(data.size());
}

}  // namespace ash::assistant
