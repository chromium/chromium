// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/virtual_file_provider/fake_virtual_file_provider_client.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeVirtualFileProviderClient::FakeVirtualFileProviderClient() = default;
FakeVirtualFileProviderClient::~FakeVirtualFileProviderClient() = default;

void FakeVirtualFileProviderClient::Init(dbus::Bus* bus) {}

void FakeVirtualFileProviderClient::GenerateVirtualFileId(
    int64_t size,
    GenerateVirtualFileIdCallback callback) {
  std::optional<std::string> id;
  if (size != expected_size_)
    LOG(ERROR) << "Unexpected size " << size << " vs " << expected_size_;
  else
    id = result_id_;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(id)));
}

void FakeVirtualFileProviderClient::OpenFileById(
    const std::string& id,
    OpenFileByIdCallback callback) {
  base::ScopedFD fd;
  if (id != result_id_)
    LOG(ERROR) << "Unexpected id " << id << " vs " << result_id_;
  else
    fd = std::move(result_fd_);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(fd)));
}

}  // namespace ash
