// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_CONTENT_INTERNAL_DOWNLOAD_DRIVER_IMPL_H_
#define COMPONENTS_DOWNLOAD_CONTENT_INTERNAL_DOWNLOAD_DRIVER_IMPL_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/download/internal/background_service/download_driver.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/common/all_download_event_notifier.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace download {

class SimpleDownloadManagerCoordinator;

struct DriverEntry;

// Aggregates and handles all interaction between download service and content
// download logic.
class DownloadDriverImpl : public DownloadDriver,
                           public AllDownloadEventNotifier::Observer {
 public:
  // Creates a driver entry based on a download item.
  static DriverEntry CreateDriverEntry(const download::DownloadItem* item);

  // Create the driver.
  DownloadDriverImpl(
      SimpleDownloadManagerCoordinator* download_manager_coordinator);
  ~DownloadDriverImpl() override;

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

 private:
  // content::AllDownloadEventNotifier::Observer implementation.
  void OnDownloadsInitialized(SimpleDownloadManagerCoordinator* coordinator,
                              bool active_downloads_only) override;
  void OnManagerGoingDown(
      SimpleDownloadManagerCoordinator* coordinator) override;
  void OnDownloadCreated(SimpleDownloadManagerCoordinator* coordinator,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(SimpleDownloadManagerCoordinator* coordinator,
                         download::DownloadItem* item) override;
  void OnDownloadRemoved(SimpleDownloadManagerCoordinator* coordinator,
                         download::DownloadItem* item) override;

  void OnUploadProgress(const std::string& guid, uint64_t bytes_uploaded);

  void OnHardRecoverComplete(bool success);

  // Remove the download, used to be posted to the task queue.
  void DoRemoveDownload(const std::string& guid, bool remove_file);

  // The client that receives updates from low level download logic.
  DownloadDriver::Client* client_;

  // Pending guid set of downloads that will be removed soon.
  std::set<std::string> guid_to_remove_;

  // Coordinator for handling the actual download when |download_manager_| is
  // no longer used.
  SimpleDownloadManagerCoordinator* download_manager_coordinator_;

  // Whether this object is ready to handle download requests.
  bool is_ready_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Only used to post tasks on the same thread.
  base::WeakPtrFactory<DownloadDriverImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadDriverImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_INTERNAL_DOWNLOAD_DRIVER_IMPL_H_
