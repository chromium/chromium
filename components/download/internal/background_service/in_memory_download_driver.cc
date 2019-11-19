// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/in_memory_download_driver.h"

#include "components/download/internal/background_service/in_memory_download.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace download {

namespace {

DriverEntry::State ToDriverEntryState(InMemoryDownload::State state) {
  switch (state) {
    case InMemoryDownload::State::INITIAL:
    case InMemoryDownload::State::RETRIEVE_BLOB_CONTEXT:
    case InMemoryDownload::State::IN_PROGRESS:
      return DriverEntry::State::IN_PROGRESS;
    case InMemoryDownload::State::FAILED:
      return DriverEntry::State::INTERRUPTED;
    case InMemoryDownload::State::COMPLETE:
      return DriverEntry::State::COMPLETE;
  }
  NOTREACHED();
  return DriverEntry::State::UNKNOWN;
}

// Helper function to create download driver entry based on in memory download.
DriverEntry CreateDriverEntry(const InMemoryDownload& download) {
  DriverEntry entry;
  entry.guid = download.guid();
  entry.state = ToDriverEntryState(download.state());
  entry.paused = download.paused();
  entry.done = entry.state == DriverEntry::State::COMPLETE ||
               entry.state == DriverEntry::State::CANCELLED;
  entry.bytes_downloaded = download.bytes_downloaded();
  entry.url_chain = download.url_chain();
  entry.response_headers = download.response_headers();
  if (entry.response_headers) {
    entry.expected_total_size = entry.response_headers->GetContentLength();
  }
  // Currently incognito mode network backend can't resume in the middle.
  entry.can_resume = false;

  if (download.state() == InMemoryDownload::State::COMPLETE) {
    auto blob_handle = download.ResultAsBlob();
    if (blob_handle)
      entry.blob_handle = base::Optional<storage::BlobDataHandle>(*blob_handle);
  }
  return entry;
}

}  // namespace

InMemoryDownloadFactory::InMemoryDownloadFactory(
    network::mojom::URLLoaderFactory* url_loader_factory,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : url_loader_factory_(url_loader_factory),
      io_task_runner_(io_task_runner) {}

InMemoryDownloadFactory::~InMemoryDownloadFactory() = default;

std::unique_ptr<InMemoryDownload> InMemoryDownloadFactory::Create(
    const std::string& guid,
    const RequestParams& request_params,
    scoped_refptr<network::ResourceRequestBody> request_body,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    InMemoryDownload::Delegate* delegate) {
  DCHECK(url_loader_factory_);
  return std::make_unique<InMemoryDownloadImpl>(
      guid, request_params, std::move(request_body), traffic_annotation,
      delegate, url_loader_factory_, io_task_runner_);
}

InMemoryDownloadDriver::InMemoryDownloadDriver(
    std::unique_ptr<InMemoryDownload::Factory> download_factory,
    BlobContextGetterFactoryPtr blob_context_getter_factory)
    : client_(nullptr),
      download_factory_(std::move(download_factory)),
      blob_context_getter_factory_(std::move(blob_context_getter_factory)) {}

InMemoryDownloadDriver::~InMemoryDownloadDriver() = default;

void InMemoryDownloadDriver::Initialize(DownloadDriver::Client* client) {
  DCHECK(!client_) << "Initialize can be called only once.";
  client_ = client;
  DCHECK(client_);
  client_->OnDriverReady(true);
}

void InMemoryDownloadDriver::HardRecover() {
  client_->OnDriverHardRecoverComplete(true);
}

bool InMemoryDownloadDriver::IsReady() const {
  return true;
}

void InMemoryDownloadDriver::Start(
    const RequestParams& request_params,
    const std::string& guid,
    const base::FilePath& file_path,
    scoped_refptr<network::ResourceRequestBody> request_body,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  std::unique_ptr<InMemoryDownload> download = download_factory_->Create(
      guid, request_params, std::move(request_body), traffic_annotation, this);
  InMemoryDownload* download_ptr = download.get();
  DCHECK(downloads_.find(guid) == downloads_.end()) << "Existing GUID found.";
  downloads_.emplace(guid, std::move(download));

  download_ptr->Start();
}

void InMemoryDownloadDriver::Remove(const std::string& guid, bool remove_file) {
  downloads_.erase(guid);
}

void InMemoryDownloadDriver::Pause(const std::string& guid) {
  auto it = downloads_.find(guid);
  if (it != downloads_.end())
    it->second->Pause();
}

void InMemoryDownloadDriver::Resume(const std::string& guid) {
  auto it = downloads_.find(guid);
  if (it != downloads_.end())
    it->second->Resume();
}

base::Optional<DriverEntry> InMemoryDownloadDriver::Find(
    const std::string& guid) {
  base::Optional<DriverEntry> entry;
  auto it = downloads_.find(guid);
  if (it != downloads_.end())
    entry = CreateDriverEntry(*it->second);
  return entry;
}

std::set<std::string> InMemoryDownloadDriver::GetActiveDownloads() {
  std::set<std::string> downloads;
  for (const auto& it : downloads_) {
    if (it.second->state() == InMemoryDownload::State::INITIAL ||
        it.second->state() == InMemoryDownload::State::IN_PROGRESS) {
      downloads.emplace(it.first);
    }
  }
  return downloads;
}

size_t InMemoryDownloadDriver::EstimateMemoryUsage() const {
  size_t memory_usage = 0u;
  for (const auto& it : downloads_) {
    memory_usage += it.second->EstimateMemoryUsage();
  }
  return memory_usage;
}

void InMemoryDownloadDriver::OnDownloadStarted(InMemoryDownload* download) {
  DCHECK(client_);
  client_->OnDownloadCreated(CreateDriverEntry(*download));
}

void InMemoryDownloadDriver::OnDownloadProgress(InMemoryDownload* download) {
  DCHECK(client_);
  client_->OnDownloadUpdated(CreateDriverEntry(*download));
}

void InMemoryDownloadDriver::OnDownloadComplete(InMemoryDownload* download) {
  DCHECK(download);
  DCHECK(client_);
  DriverEntry entry = CreateDriverEntry(*download);
  switch (download->state()) {
    case InMemoryDownload::State::FAILED:
      // URLFetcher retries for network failures.
      client_->OnDownloadFailed(entry, FailureType::NOT_RECOVERABLE);
      // Should immediately return in case |client_| removes |download| in
      // OnDownloadFailed.
      return;
    case InMemoryDownload::State::COMPLETE:
      client_->OnDownloadSucceeded(entry);
      // Should immediately return in case |client_| removes |download| in
      // OnDownloadSucceeded.
      return;
    case InMemoryDownload::State::INITIAL:
    case InMemoryDownload::State::RETRIEVE_BLOB_CONTEXT:
    case InMemoryDownload::State::IN_PROGRESS:
      NOTREACHED();
      return;
  }
}

void InMemoryDownloadDriver::OnUploadProgress(InMemoryDownload* download) {
  DCHECK(download);
  DCHECK(client_);
  client_->OnUploadProgress(download->guid(), download->bytes_uploaded());
}

void InMemoryDownloadDriver::RetrieveBlobContextGetter(
    BlobContextGetterCallback callback) {
  blob_context_getter_factory_->RetrieveBlobContextGetter(std::move(callback));
}

}  // namespace download
