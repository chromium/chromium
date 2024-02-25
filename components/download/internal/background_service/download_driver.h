// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_DRIVER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_DRIVER_H_

#include <stddef.h>

#include <optional>
#include <set>
#include <string>

#include "components/download/internal/background_service/driver_entry.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace base {
class FilePath;
}  // namespace base

namespace network {
class ResourceRequestBody;
}  // namespace network

namespace download {

struct RequestParams;

// Indicates the recovery type of a download.
enum class FailureType {
  // Download failed due to an irrecoverable error.
  NOT_RECOVERABLE = 0,

  // Download failed, but we might be able to recover if we try again.
  RECOVERABLE = 1,
};

// The interface that includes all the operations to interact with low level
// download library functionalities.
class DownloadDriver {
 public:
  // The client to receive updates from content download library.
  // The update events for all downloads will pass through, so it's the
  // client's responsibility to filter the events it needs to handle.
  class Client {
   public:
    // Called when the low level download library is ready. |success| is true
    // when the low level download library is ready.
    virtual void OnDriverReady(bool success) = 0;

    // Called asynchronously in response to a DownloadDriver::HardRecover call.
    // If |success| is |false|, recovery of the DownloadDriver failed.
    virtual void OnDriverHardRecoverComplete(bool success) = 0;

    // Called when any download is created.
    virtual void OnDownloadCreated(const DriverEntry& download) = 0;

    // Called when any download is failed. |reason| is propagated from low level
    // download library.
    virtual void OnDownloadFailed(const DriverEntry& download,
                                  FailureType failure_type) = 0;

    // Called when any download is successfully completed.
    virtual void OnDownloadSucceeded(const DriverEntry& download) = 0;

    // Called when any download is updated.
    virtual void OnDownloadUpdated(const DriverEntry& download) = 0;

    // Returns whether the client is tracking the download with |guid|.
    virtual bool IsTrackingDownload(const std::string& guid) const = 0;

    // Called when progress has been made in uploading the body for the download
    // with |guid|.
    virtual void OnUploadProgress(const std::string& guid,
                                  uint64_t bytes_uploaded) const = 0;
  };

  virtual ~DownloadDriver() = default;

  // Initialize the driver to receive download updates.
  virtual void Initialize(Client* client) = 0;

  // Attempts to clean up and reset the DownloadDriver.  It should remove all
  // state relevant to the DownloadService.
  virtual void HardRecover() = 0;

  // Returns if the driver is ready after the low level library has loaded all
  // the data. Returns false when the driver is not initialized by the client,
  // or low level download library has been shut down.
  virtual bool IsReady() const = 0;

  // Starts a new download.
  virtual void Start(
      const RequestParams& request_params,
      const std::string& guid,
      const base::FilePath& file_path,
      scoped_refptr<network::ResourceRequestBody> post_body,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) = 0;

  // Cancels an existing download, all data associated with this download should
  // be removed. If download is not completed, the temporary file will be
  // deleted. If download is completed, the download file will be deleted when
  // |remove_file| is true.
  virtual void Remove(const std::string& guid, bool remove_file) = 0;

  // Pauses the download.
  virtual void Pause(const std::string& guid) = 0;

  // Resumes the download
  virtual void Resume(const std::string& guid) = 0;

  // Finds a download record from low level download library.
  virtual std::optional<DriverEntry> Find(const std::string& guid) = 0;

  // Called to query the current set of active downloads.  This doesn't
  // necessarily mean downloads started by the service.
  virtual std::set<std::string> GetActiveDownloads() = 0;

  // Returns the estimate of dynamically allocated memory in bytes.
  virtual size_t EstimateMemoryUsage() const = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_DOWNLOAD_DRIVER_H_
