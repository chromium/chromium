// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dlp/fake_dlp_client.h"

#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"

namespace chromeos {

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

void FakeDlpClient::AddFiles(const dlp::AddFilesRequest request,
                             AddFilesCallback callback) {
  std::vector<base::FilePath> added_files;
  for (const dlp::AddFileRequest& file_request : request.add_file_requests()) {
    added_files.emplace_back(file_request.file_path());
  }

  for (auto& observer : observers_) {
    observer.OnFilesAddedToDlpDaemon(added_files);
  }

  if (add_files_mock_.has_value()) {
    add_files_mock_->Run(request, std::move(callback));
    return;
  }
  for (const dlp::AddFileRequest& file_request : request.add_file_requests()) {
    if (file_request.has_file_path() && file_request.has_source_url()) {
      files_database_[file_request.file_path()] = std::make_pair(
          file_request.source_url(), file_request.referrer_url());
    }
  }

  dlp::AddFilesResponse response;
  std::move(callback).Run(response);
}

void FakeDlpClient::GetFilesSources(const dlp::GetFilesSourcesRequest request,
                                    GetFilesSourcesCallback callback) {
  if (get_files_source_mock_.has_value()) {
    get_files_source_mock_->Run(request, std::move(callback));
    return;
  }
  dlp::GetFilesSourcesResponse response;
  for (const auto& file_path : request.files_paths()) {
    auto file_itr = files_database_.find(file_path);
    if (file_itr == files_database_.end() && !fake_source_.has_value()) {
      continue;
    }

    dlp::FileMetadata* file_metadata = response.add_files_metadata();
    file_metadata->set_path(file_path);
    file_metadata->set_source_url(
        fake_source_.value_or(file_itr->second.first));
    file_metadata->set_referrer_url(file_itr->second.second);
  }
  std::move(callback).Run(response);
}

void FakeDlpClient::CheckFilesTransfer(
    const dlp::CheckFilesTransferRequest request,
    CheckFilesTransferCallback callback) {
  if (check_files_transfer_mock_.has_value()) {
    check_files_transfer_mock_->Run(request, std::move(callback));
    return;
  }
  last_check_files_transfer_request_ = request;
  dlp::CheckFilesTransferResponse response;
  if (check_files_transfer_response_.has_value()) {
    response = check_files_transfer_response_.value();
  }
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

void FakeDlpClient::GetDatabaseEntries(GetDatabaseEntriesCallback callback) {
  dlp::GetDatabaseEntriesResponse response;
  for (auto& [path, urls] : files_database_) {
    auto* file_entry = response.add_files_entries();
    file_entry->set_source_url(urls.first);
    file_entry->set_referrer_url(urls.second);
    file_entry->set_path(path);
  }
  std::move(callback).Run(response);
}

bool FakeDlpClient::IsAlive() const {
  return is_alive_;
}

void FakeDlpClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeDlpClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeDlpClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
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

void FakeDlpClient::SetAddFilesMock(AddFilesCall mock) {
  add_files_mock_ = mock;
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

void FakeDlpClient::SetCheckFilesTransferMock(CheckFilesTransferCall mock) {
  check_files_transfer_mock_ = std::move(mock);
}

}  // namespace chromeos
