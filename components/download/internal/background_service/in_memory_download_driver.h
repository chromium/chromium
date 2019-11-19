// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IN_MEMORY_DOWNLOAD_DRIVER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IN_MEMORY_DOWNLOAD_DRIVER_H_

#include "components/download/internal/background_service/download_driver.h"

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "components/download/internal/background_service/in_memory_download.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace download {

class InMemoryDownload;

// Factory to create in memory download object.
class InMemoryDownloadFactory : public InMemoryDownload::Factory {
 public:
  InMemoryDownloadFactory(
      network::mojom::URLLoaderFactory* url_loader_factory,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~InMemoryDownloadFactory() override;

 private:
  // InMemoryDownload::Factory implementation.
  std::unique_ptr<InMemoryDownload> Create(
      const std::string& guid,
      const RequestParams& request_params,
      scoped_refptr<network::ResourceRequestBody> request_body,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      InMemoryDownload::Delegate* delegate) override;

  network::mojom::URLLoaderFactory* url_loader_factory_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryDownloadFactory);
};

// Download backend that owns the list of in memory downloads and propagate
// notification to its client.
class InMemoryDownloadDriver : public DownloadDriver,
                               public InMemoryDownload::Delegate {
 public:
  InMemoryDownloadDriver(
      std::unique_ptr<InMemoryDownload::Factory> download_factory,
      BlobContextGetterFactoryPtr blob_context_getter_factory);
  ~InMemoryDownloadDriver() override;

 private:
  // DownloadDriver implementation.
  void Initialize(DownloadDriver::Client* client) override;
  void HardRecover() override;
  bool IsReady() const override;
  void Start(
      const RequestParams& request_params,
      const std::string& guid,
      const base::FilePath& file_path,
      scoped_refptr<network::ResourceRequestBody> post_body,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  void Remove(const std::string& guid, bool remove_file) override;
  void Pause(const std::string& guid) override;
  void Resume(const std::string& guid) override;
  base::Optional<DriverEntry> Find(const std::string& guid) override;
  std::set<std::string> GetActiveDownloads() override;
  size_t EstimateMemoryUsage() const override;

  // InMemoryDownload::Delegate implementation.
  void OnDownloadStarted(InMemoryDownload* download) override;
  void OnDownloadProgress(InMemoryDownload* download) override;
  void OnDownloadComplete(InMemoryDownload* download) override;
  void OnUploadProgress(InMemoryDownload* download) override;
  void RetrieveBlobContextGetter(BlobContextGetterCallback callback) override;

  // The client that receives updates from low level download logic.
  DownloadDriver::Client* client_;

  // The factory used to create in memory download objects.
  std::unique_ptr<InMemoryDownload::Factory> download_factory_;

  // Used to retrieve BlobStorageContextGetter.
  BlobContextGetterFactoryPtr blob_context_getter_factory_;

  // A map of GUID and in memory download, which holds download data.
  std::map<std::string, std::unique_ptr<InMemoryDownload>> downloads_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryDownloadDriver);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IN_MEMORY_DOWNLOAD_DRIVER_H_
