// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/ipc_data_source.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

IPCDataSource::IPCDataSource(
    mojo::PendingRemote<chrome::mojom::MediaDataSource> media_data_source,
    int64_t total_size)
    : media_data_source_(std::move(media_data_source)),
      total_size_(total_size),
      utility_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
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
                     destination, std::move(callback), position, size));
}

bool IPCDataSource::GetSize(int64_t* size_out) {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);
  *size_out = total_size_;
  return true;
}

bool IPCDataSource::IsStreaming() {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);
  return false;
}

void IPCDataSource::SetBitrate(int bitrate) {
  DCHECK_CALLED_ON_VALID_THREAD(data_source_thread_checker_);
}

void IPCDataSource::ReadMediaData(uint8_t* destination,
                                  DataSource::ReadCB callback,
                                  int64_t position,
                                  int size) {
  DCHECK_CALLED_ON_VALID_THREAD(utility_thread_checker_);
  CHECK_GE(total_size_, 0);
  CHECK_GE(position, 0);
  CHECK_GE(size, 0);

  // Cap position and size within bounds.
  position = std::min(position, total_size_);
  int64_t clamped_size =
      std::min(static_cast<int64_t>(size), total_size_ - position);

  media_data_source_->Read(
      position, clamped_size,
      base::BindOnce(&IPCDataSource::ReadDone, base::Unretained(this),
                     destination, std::move(callback)));
}

void IPCDataSource::ReadDone(uint8_t* destination,
                             DataSource::ReadCB callback,
                             const std::vector<uint8_t>& data) {
  DCHECK_CALLED_ON_VALID_THREAD(utility_thread_checker_);

  std::copy(data.begin(), data.end(), destination);
  std::move(callback).Run(data.size());
}
