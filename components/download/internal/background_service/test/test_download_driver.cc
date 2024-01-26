// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/test/test_download_driver.h"

#include "base/files/file_path.h"
#include "components/download/public/background_service/download_params.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace download {
namespace test {

TestDownloadDriver::TestDownloadDriver() : is_ready_(false), client_(nullptr) {}

TestDownloadDriver::~TestDownloadDriver() = default;

void TestDownloadDriver::MakeReady() {
  is_ready_ = true;
  DCHECK(client_);
  if (client_)
    client_->OnDriverReady(is_ready_);
}

void TestDownloadDriver::TriggerHardRecoverComplete(bool success) {
  if (client_)
    client_->OnDriverHardRecoverComplete(success);
}

void TestDownloadDriver::AddTestData(const std::vector<DriverEntry>& entries) {
  for (const auto& entry : entries) {
    DCHECK(entries_.find(entry.guid) == entries_.end()) << "Existing guid.";
    entries_.emplace(entry.guid, entry);
  }
}

void TestDownloadDriver::NotifyDownloadUpdate(const DriverEntry& entry) {
  if (client_) {
    entries_[entry.guid] = entry;
    client_->OnDownloadUpdated(entry);
  }
}

void TestDownloadDriver::NotifyDownloadFailed(const DriverEntry& entry,
                                              FailureType failure_type) {
  if (client_) {
    entries_[entry.guid] = entry;
    client_->OnDownloadFailed(entry, failure_type);
  }
}

void TestDownloadDriver::NotifyDownloadSucceeded(const DriverEntry& entry) {
  if (client_) {
    entries_[entry.guid] = entry;
    client_->OnDownloadSucceeded(entry);
  }
}

void TestDownloadDriver::Initialize(DownloadDriver::Client* client) {
  DCHECK(!client_);
  client_ = client;
}

void TestDownloadDriver::HardRecover() {}

bool TestDownloadDriver::IsReady() const {
  return is_ready_;
}

void TestDownloadDriver::Start(
    const RequestParams& params,
    const std::string& guid,
    const base::FilePath& file_path,
    scoped_refptr<network::ResourceRequestBody> post_body,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DriverEntry entry;
  entry.guid = guid;
  entry.current_file_path = file_path;
  entry.state = DriverEntry::State::IN_PROGRESS;
  entry.paused = false;
  entry.bytes_downloaded = 0;
  entry.expected_total_size = 0;
  entry.url_chain = {params.url};
  entry.response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200");
  entries_[guid] = entry;

  if (client_)
    client_->OnDownloadCreated(entry);
}

void TestDownloadDriver::Remove(const std::string& guid, bool remove_file) {
  entries_.erase(guid);
}

void TestDownloadDriver::Pause(const std::string& guid) {
  auto it = entries_.find(guid);
  if (it == entries_.end())
    return;
  it->second.paused = true;
}

void TestDownloadDriver::Resume(const std::string& guid) {
  auto it = entries_.find(guid);
  if (it == entries_.end())
    return;
  it->second.paused = false;
}

std::optional<DriverEntry> TestDownloadDriver::Find(const std::string& guid) {
  auto it = entries_.find(guid);
  if (it == entries_.end())
    return std::nullopt;
  return it->second;
}

std::set<std::string> TestDownloadDriver::GetActiveDownloads() {
  std::set<std::string> guids;

  for (auto& entry : entries_) {
    if (entry.second.state == DriverEntry::State::IN_PROGRESS)
      guids.insert(entry.second.guid);
  }

  return guids;
}

size_t TestDownloadDriver::EstimateMemoryUsage() const {
  return 0u;
}

}  // namespace test
}  // namespace download
