// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/virtual_file_provider/fake_virtual_file_provider_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeVirtualFileProviderClient::FakeVirtualFileProviderClient() = default;
FakeVirtualFileProviderClient::~FakeVirtualFileProviderClient() = default;

void FakeVirtualFileProviderClient::Init(dbus::Bus* bus) {}

void FakeVirtualFileProviderClient::GenerateVirtualFileId(
    int64_t size,
    GenerateVirtualFileIdCallback callback) {
  absl::optional<std::string> id;
  if (size != expected_size_)
    LOG(ERROR) << "Unexpected size " << size << " vs " << expected_size_;
  else
    id = result_id_;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
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

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(fd)));
}

}  // namespace chromeos
