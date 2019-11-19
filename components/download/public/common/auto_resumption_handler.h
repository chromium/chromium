// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_AUTO_RESUMPTION_HANDLER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_AUTO_RESUMPTION_HANDLER_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/network/network_status_listener.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/task/task_manager.h"

namespace download {

// Handles auto-resumptions for downloads. Listens to network changes and
// schecules task to resume downloads accordingly.
class COMPONENTS_DOWNLOAD_EXPORT AutoResumptionHandler
    : public download::NetworkStatusListener::Observer,
      public download::DownloadItem::Observer {
 public:
  struct COMPONENTS_DOWNLOAD_EXPORT Config {
    Config();
    ~Config() = default;

    int auto_resumption_size_limit;
    bool is_auto_resumption_enabled_in_native;
  };

  // Creates the singleton instance of AutoResumptionHandler.
  static void Create(
      std::unique_ptr<download::NetworkStatusListener> network_listener,
      std::unique_ptr<download::TaskManager> task_manager,
      std::unique_ptr<Config> config);

  // Returns the singleton instance of the AutoResumptionHandler, or nullptr if
  // initialization is not yet complete.
  static AutoResumptionHandler* Get();

  // Utility function to determine whether an interrupted download should be
  // auto-resumable.
  static bool IsInterruptedDownloadAutoResumable(
      download::DownloadItem* download_item,
      int auto_resumption_size_limit);

  AutoResumptionHandler(
      std::unique_ptr<download::NetworkStatusListener> network_listener,
      std::unique_ptr<download::TaskManager> task_manager,
      std::unique_ptr<Config> config);
  ~AutoResumptionHandler() override;

  void SetResumableDownloads(
      const std::vector<download::DownloadItem*>& downloads);
  bool IsActiveNetworkMetered() const;
  void OnStartScheduledTask(download::TaskFinishedCallback callback);
  bool OnStopScheduledTask();

  void OnDownloadStarted(download::DownloadItem* item);

  // DownloadItem::Observer overrides.
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadRemoved(download::DownloadItem* item) override;
  void OnDownloadDestroyed(download::DownloadItem* item) override;

 private:
  // NetworkStatusListener::Observer implementation.
  void OnNetworkChanged(network::mojom::ConnectionType type) override;

  void ResumePendingDownloads();
  void RecomputeTaskParams();
  void RescheduleTaskIfNecessary();
  void ResumeDownloadImmediately();
  bool SatisfiesNetworkRequirements(download::DownloadItem* download);
  bool IsAutoResumableDownload(download::DownloadItem* item);

  std::unique_ptr<download::NetworkStatusListener> network_listener_;

  std::unique_ptr<download::TaskManager> task_manager_;

  std::unique_ptr<Config> config_;

  // List of downloads that are auto-resumable. These will be resumed as soon as
  // network conditions becomes favorable.
  std::map<std::string, download::DownloadItem*> resumable_downloads_;

  // A temporary list of downloads which are being retried immediately.
  std::set<download::DownloadItem*> downloads_to_retry_;

  bool recompute_task_params_scheduled_ = false;

  base::WeakPtrFactory<AutoResumptionHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutoResumptionHandler);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_AUTO_RESUMPTION_HANDLER_H_
