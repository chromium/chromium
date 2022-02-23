// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dlp/fake_dlp_client.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"

namespace chromeos {

namespace {

ino_t GetInodeValue(const base::FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0)
    return 0;
  return file_stats.st_ino;
}

}  // namespace

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
                            AddFileCallback callback) {
  if (request.has_file_path() && request.has_source_url()) {
    files_database_[GetInodeValue(base::FilePath(request.file_path()))] =
        request.source_url();
  }
  dlp::AddFileResponse response;
  std::move(callback).Run(response);
}

void FakeDlpClient::GetFilesSources(const dlp::GetFilesSourcesRequest request,
                                    GetFilesSourcesCallback callback) const {
  dlp::GetFilesSourcesResponse response;
  for (const auto& file_inode : request.files_inodes()) {
    auto file_itr = files_database_.find(file_inode);
    if (file_itr == files_database_.end() && !fake_source_.has_value())
      continue;

    dlp::FileMetadata* file_metadata = response.add_files_metadata();
    file_metadata->set_inode(file_inode);
    file_metadata->set_source_url(fake_source_.value_or(file_itr->second));
  }
  std::move(callback).Run(response);
}

bool FakeDlpClient::IsAlive() const {
  return true;
}

DlpClient::TestInterface* FakeDlpClient::GetTestInterface() {
  return this;
}

int FakeDlpClient::GetSetDlpFilesPolicyCount() const {
  return set_dlp_files_policy_count_;
}

void FakeDlpClient::SetFakeSource(const std::string& fake_source) {
  fake_source_ = fake_source;
}

}  // namespace chromeos
