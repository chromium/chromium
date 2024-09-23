// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/browser/download/notification/multi_profile_download_notifier.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"

namespace content {
class DownloadManager;
}  // namespace content

namespace ash {

namespace holding_space_metrics {
enum class EventSource;
}  // namespace holding_space_metrics

// A delegate of `HoldingSpaceKeyedService` tasked with monitoring the status of
// of downloads on its behalf.
class HoldingSpaceDownloadsDelegate
    : public HoldingSpaceKeyedServiceDelegate,
      public MultiProfileDownloadNotifier::Client,
      public arc::ArcFileSystemBridge::Observer {
 public:
  HoldingSpaceDownloadsDelegate(HoldingSpaceKeyedService* service,
                                HoldingSpaceModel* model);
  HoldingSpaceDownloadsDelegate(const HoldingSpaceDownloadsDelegate&) = delete;
  HoldingSpaceDownloadsDelegate& operator=(
      const HoldingSpaceDownloadsDelegate&) = delete;
  ~HoldingSpaceDownloadsDelegate() override;

  // Attempts to mark the download underlying the given `item` to open when
  // complete. Returns `std::nullopt` on success or the reason if the attempt
  // was not successful.
  std::optional<holding_space_metrics::ItemLaunchFailureReason>
  OpenWhenComplete(const HoldingSpaceItem* item);

 private:
  class InProgressDownload;

  // HoldingSpaceKeyedServiceDelegate:
  void OnPersistenceRestored() override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;

  // MultiProfileDownloadNotifier::Client:
  void OnManagerInitialized(content::DownloadManager* manager) override;
  void OnManagerGoingDown(content::DownloadManager* manager) override;
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

  // arc::ArcFileSystemBridge::Observer:
  void OnMediaStoreUriAdded(
      const GURL& uri,
      const arc::mojom::MediaStoreMetadata& metadata) override;

  // Invoked when the specified `in_progress_download` is updated. If
  // `invalidate_image` is `true`, the image for the associated holding space
  // item will be explicitly invalidated. This is necessary if, for example, the
  // underlying download is transitioning to/from a dangerous or insecure state.
  void OnDownloadUpdated(InProgressDownload* in_progress_download,
                         bool invalidate_image);

  // Invoked when the specified `in_progress_download` is completed.
  void OnDownloadCompleted(InProgressDownload* in_progress_download);

  // Invoked when the specified `in_progress_download` fails. This may be due to
  // cancellation, interruption, or destruction of the underlying download.
  void OnDownloadFailed(const InProgressDownload* in_progress_download);

  // Invoked to erase the specified `in_progress_download` when it is no longer
  // needed either due to completion or failure of the underlying download.
  void EraseDownload(const InProgressDownload* in_progress_download);

  // Creates or updates the holding space item in the model associated with the
  // specified `in_progress_download`. If `invalidate_image` is `true`, the
  // image for the holding space item will be explicitly invalidated. This is
  // necessary if, for example, the underlying download is transitioning to/from
  // a dangerous or insecure state.
  void CreateOrUpdateHoldingSpaceItem(InProgressDownload* in_progress_download,
                                      bool invalidate_image);

  // Attempts to cancel/pause/resume the download underlying the given `item`.
  void Cancel(const HoldingSpaceItem* item,
              HoldingSpaceCommandId command_id,
              holding_space_metrics::EventSource event_source);
  void Pause(const HoldingSpaceItem* item,
             HoldingSpaceCommandId command_id,
             holding_space_metrics::EventSource event_source);
  void Resume(const HoldingSpaceItem* item,
              HoldingSpaceCommandId command_id,
              holding_space_metrics::EventSource event_source);

  // The collection of currently in-progress downloads.
  std::set<std::unique_ptr<InProgressDownload>, base::UniquePtrComparator>
      in_progress_downloads_;

  base::ScopedObservation<arc::ArcFileSystemBridge,
                          arc::ArcFileSystemBridge::Observer>
      arc_file_system_bridge_observation_{this};

  // Notifies this delegate of download events created for the profile
  // associated with this delegate's service. If the incognito profile
  // integration feature is enabled, the delegate is also notified of download
  // events created for incognito profiles spawned from the service's main
  // profile.
  MultiProfileDownloadNotifier download_notifier_{
      this, /*wait_for_manager_initialization=*/true};

  base::WeakPtrFactory<HoldingSpaceDownloadsDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_
