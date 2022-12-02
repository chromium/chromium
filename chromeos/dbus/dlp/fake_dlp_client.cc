// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dlp/fake_dlp_client.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), response));
}

void FakeDlpClient::AddFile(const dlp::AddFileRequest request,
                            AddFileCallback callback) {
  if (add_file_mock_.has_value()) {
    add_file_mock_->Run(request, std::move(callback));
    return;
  }
  if (request.has_file_path() && request.has_source_url()) {
    files_database_[GetInodeValue(base::FilePath(request.file_path()))] =
        request.source_url();
  }
  dlp::AddFileResponse response;
  std::move(callback).Run(response);
}

void FakeDlpClient::GetFilesSources(const dlp::GetFilesSourcesRequest request,
                                    GetFilesSourcesCallback callback) {
  if (get_files_source_mock_.has_value()) {
    get_files_source_mock_->Run(request, std::move(callback));
    return;
  }
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

void FakeDlpClient::CheckFilesTransfer(
    const dlp::CheckFilesTransferRequest request,
    CheckFilesTransferCallback callback) {
  last_check_files_transfer_request_ = request;
  dlp::CheckFilesTransferResponse response;
  if (check_files_transfer_response_.has_value())
    response = check_files_transfer_response_.value();
  std::move(callback).Run(response);
}

void FakeDlpClient::RequestFileAccess(
    const dlp::RequestFileAccessRequest request,
    RequestFileAccessCallback callback) {
  if (request_file_access_mock_.has_value()) {
    request_file_access_mock_->Run(request, std::move(callback));
    return;
  }
  dlp::RequestFileAccessResponse response;
  response.set_allowed(file_access_allowed_);
  std::move(callback).Run(response, base::ScopedFD());
}

bool FakeDlpClient::IsAlive() const {
  return is_alive_;
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

void FakeDlpClient::SetCheckFilesTransferResponse(
    dlp::CheckFilesTransferResponse response) {
  check_files_transfer_response_ = response;
}

void FakeDlpClient::SetFileAccessAllowed(bool allowed) {
  file_access_allowed_ = allowed;
}

void FakeDlpClient::SetIsAlive(bool is_alive) {
  is_alive_ = is_alive;
}

void FakeDlpClient::SetAddFileMock(AddFileCall mock) {
  add_file_mock_ = mock;
}

void FakeDlpClient::SetGetFilesSourceMock(GetFilesSourceCall mock) {
  get_files_source_mock_ = mock;
}

dlp::CheckFilesTransferRequest FakeDlpClient::GetLastCheckFilesTransferRequest()
    const {
  return last_check_files_transfer_request_;
}

void FakeDlpClient::SetRequestFileAccessMock(RequestFileAccessCall mock) {
  request_file_access_mock_ = std::move(mock);
}

}  // namespace chromeos
