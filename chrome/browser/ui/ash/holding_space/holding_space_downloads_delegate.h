// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_

#include "base/callback.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/intent_helper/arc_intent_helper_observer.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_manager.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// A delegate of `HoldingSpaceKeyedService` tasked with monitoring the status of
// of downloads on its behalf.
class HoldingSpaceDownloadsDelegate : public HoldingSpaceKeyedServiceDelegate,
                                      public arc::ArcIntentHelperObserver,
                                      public content::DownloadManager::Observer,
                                      public download::DownloadItem::Observer {
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

 private:
  // HoldingSpaceKeyedServiceDelegate:
  void Init() override;
  void OnPersistenceRestored() override;

  // arc::ArcIntentHelperObserver:
  void OnArcDownloadAdded(const base::FilePath& relative_path,
                          const std::string& owner_package_name) override;

  // content::DownloadManager::Observer:
  void OnManagerInitialized() override;
  void ManagerGoingDown(content::DownloadManager* manager) override;
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* item) override;

  // Invoked when a download of the specified `type` at the specified
  // `file_path` has completed downloading. Note that the specified `type` must
  // be a download type.
  void OnDownloadCompleted(HoldingSpaceItem::Type type,
                           const base::FilePath& file_path);

  base::ScopedObservation<arc::ArcIntentHelperBridge,
                          arc::ArcIntentHelperObserver>
      arc_intent_helper_observation_{this};

  base::ScopedObservation<content::DownloadManager,
                          content::DownloadManager::Observer>
      download_manager_observation_{this};

  base::ScopedMultiSourceObservation<download::DownloadItem,
                                     download::DownloadItem::Observer>
      download_item_observations_{this};

  base::WeakPtrFactory<HoldingSpaceDownloadsDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_
