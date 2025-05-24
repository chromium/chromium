// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_ANDROID_AUTO_RESUMPTION_HANDLER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_ANDROID_AUTO_RESUMPTION_HANDLER_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/download/network/network_status_listener.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/task/task_manager.h"

namespace base {
class Clock;
}  // namespace base

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
      std::unique_ptr<Config> config,
      base::Clock* clock);

  // Returns the singleton instance of the AutoResumptionHandler, or nullptr if
  // initialization is not yet complete.
  static AutoResumptionHandler* Get();

  AutoResumptionHandler(
      std::unique_ptr<download::NetworkStatusListener> network_listener,
      std::unique_ptr<download::TaskManager> task_manager,
      std::unique_ptr<Config> config,
      base::Clock* clock);

  AutoResumptionHandler(const AutoResumptionHandler&) = delete;
  AutoResumptionHandler& operator=(const AutoResumptionHandler&) = delete;

  ~AutoResumptionHandler() override;

  void SetResumableDownloads(
      const std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>&
          downloads);
  bool IsActiveNetworkMetered() const;
  void OnStartScheduledTask(DownloadTaskType type,
                            TaskFinishedCallback callback);
  bool OnStopScheduledTask(DownloadTaskType type);

  void OnDownloadStarted(download::DownloadItem* item);

  // DownloadItem::Observer overrides.
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadRemoved(download::DownloadItem* item) override;
  void OnDownloadDestroyed(download::DownloadItem* item) override;

 private:
  using DownloadMap =
      std::map<std::string, raw_ptr<DownloadItem, CtnExperimental>>;

  // NetworkStatusListener::Observer implementation.
  void OnNetworkStatusReady(network::mojom::ConnectionType type) override;
  void OnNetworkChanged(network::mojom::ConnectionType type) override;

  void ResumePendingDownloads();

  // Maybe resume some of the |downloads|. Returns the number of downloads
  // resumed. Pass by value is intentional to avoid concurrent modification.
  int MaybeResumeDownloads(DownloadMap downloads);

  void RecomputeTaskParams();
  void RescheduleTaskIfNecessary();
  void RescheduleTaskIfNecessaryForTaskType(DownloadTaskType task_type);
  void ResumeDownloadImmediately();
  bool ShouldResumeNow(download::DownloadItem* download) const;
  bool IsAutoResumableDownload(download::DownloadItem* item) const;

  // Listens to network events to stop/resume downloads accordingly.
  std::unique_ptr<download::NetworkStatusListener> network_listener_;

  std::unique_ptr<download::TaskManager> task_manager_;

  std::unique_ptr<Config> config_;

  raw_ptr<base::Clock> clock_;

  // List of downloads that are auto-resumable. These will be resumed as soon as
  // network conditions becomes favorable.
  DownloadMap resumable_downloads_;

  // A temporary list of downloads which are being retried immediately.
  std::set<raw_ptr<download::DownloadItem, SetExperimental>>
      downloads_to_retry_;

  bool recompute_task_params_scheduled_ = false;

  base::WeakPtrFactory<AutoResumptionHandler> weak_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_ANDROID_AUTO_RESUMPTION_HANDLER_H_
