// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_

#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crosapi/download_controller_ash.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"
#include "chromeos/crosapi/mojom/download_controller.mojom-forward.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "content/public/browser/download_manager.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// A delegate of `HoldingSpaceKeyedService` tasked with monitoring the status of
// of downloads on its behalf.
class HoldingSpaceDownloadsDelegate
    : public HoldingSpaceKeyedServiceDelegate,
      public arc::ArcIntentHelperObserver,
      public content::DownloadManager::Observer,
      public crosapi::DownloadControllerAsh::DownloadControllerObserver {
 public:
  HoldingSpaceDownloadsDelegate(HoldingSpaceKeyedService* service,
                                HoldingSpaceModel* model);
  HoldingSpaceDownloadsDelegate(const HoldingSpaceDownloadsDelegate&) = delete;
  HoldingSpaceDownloadsDelegate& operator=(
      const HoldingSpaceDownloadsDelegate&) = delete;
  ~HoldingSpaceDownloadsDelegate() override;

  // Sets the `content::DownloadManager` to be used for testing.
  // NOTE: This method must be called prior to delegate initialization.
  static void SetDownloadManagerForTesting(
      content::DownloadManager* download_manager);

  // Attempts to cancel/pause/resume the download underlying the given `item`.
  void Cancel(const HoldingSpaceItem* item);
  void Pause(const HoldingSpaceItem* item);
  void Resume(const HoldingSpaceItem* item);

  // Attempts to mark the download underlying the given `item` to be opened when
  // complete, returning whether or not the attempt was successful.
  bool OpenWhenComplete(const HoldingSpaceItem* item);

 private:
  class InProgressDownload;

  // HoldingSpaceKeyedServiceDelegate:
  void Init() override;
  void OnPersistenceRestored() override;
  void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) override;

  // arc::ArcIntentHelperObserver:
  void OnArcDownloadAdded(const base::FilePath& relative_path,
                          const std::string& owner_package_name) override;

  // content::DownloadManager::Observer:
  void OnManagerInitialized() override;
  void ManagerGoingDown(content::DownloadManager* manager) override;
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* download_item) override;

  // crosapi::DownloadControllerAsh::DownloadControllerObserver:
  void OnLacrosDownloadUpdated(
      const crosapi::mojom::DownloadEvent& event) override;

  // Invoked when the specified `in_progress_download` is updated.
  void OnDownloadUpdated(InProgressDownload* in_progress_download);

  // Invoked when the specified `in_progress_download` is completed.
  void OnDownloadCompleted(InProgressDownload* in_progress_download);

  // Invoked when the specified `in_progress_download` fails. This may be due to
  // cancellation, interruption, or destruction of the underlying download.
  void OnDownloadFailed(const InProgressDownload* in_progress_download);

  // Invoked to erase the specified `in_progress_download` when it is no longer
  // needed either due to completion or failure of the underlying download.
  void EraseDownload(const InProgressDownload* in_progress_download);

  // Creates or updates the holding space item in the model associated with the
  // specified `in_progress_download`.
  void CreateOrUpdateHoldingSpaceItem(InProgressDownload* in_progress_download);

  // The collection of currently in-progress downloads.
  std::set<std::unique_ptr<InProgressDownload>, base::UniquePtrComparator>
      in_progress_downloads_;

  base::ScopedObservation<arc::ArcIntentHelperBridge,
                          arc::ArcIntentHelperObserver>
      arc_intent_helper_observation_{this};

  base::ScopedObservation<content::DownloadManager,
                          content::DownloadManager::Observer>
      download_manager_observation_{this};

  base::WeakPtrFactory<HoldingSpaceDownloadsDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_
