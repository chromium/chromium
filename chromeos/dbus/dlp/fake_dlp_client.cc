// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dlp/fake_dlp_client.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"

namespace chromeos {

FakeDlpClient::FakeDlpClient() = default;

FakeDlpClient::~FakeDlpClient() = default;

void FakeDlpClient::SetDlpFilesPolicy(
    const dlp::SetDlpFilesPolicyRequest request,
    SetDlpFilesPolicyCallback callback) {
  ++set_dlp_files_policy_count_;
  dlp::SetDlpFilesPolicyResponse response;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeDlpClient::AddFile(const dlp::AddFileRequest request,
                            AddFileCallback callback) {}

void FakeDlpClient::GetFilesSources(const dlp::GetFilesSourcesRequest request,
                                    GetFilesSourcesCallback callback) const {}

bool FakeDlpClient::IsAlive() const {
  return false;
}

DlpClient::TestInterface* FakeDlpClient::GetTestInterface() {
  return this;
}

int FakeDlpClient::GetSetDlpFilesPolicyCount() const {
  return set_dlp_files_policy_count_;
}

}  // namespace chromeos
