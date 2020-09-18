// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_manager.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// A delegate of `HoldingSpaceKeyedService` tasked with monitoring the status of
// of downloads and notifying a callback on download completion.
class HoldingSpaceDownloadsDelegate : public HoldingSpaceKeyedServiceDelegate,
                                      public content::DownloadManager::Observer,
                                      public download::DownloadItem::Observer {
 public:
  // Callback to be invoked when a download is completed. Note that this
  // callback will only be invoked after holding space persistence is restored.
  using ItemDownloadedCallback =
      base::RepeatingCallback<void(const base::FilePath&)>;

  // Callback to invoke when all downloads have been restored to holding space.
  using DownloadsRestoredCallback = base::OnceClosure;

  HoldingSpaceDownloadsDelegate(
      Profile* profile,
      HoldingSpaceModel* model,
      ItemDownloadedCallback item_downloaded_callback,
      DownloadsRestoredCallback downloads_restored_callback);
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
  void Shutdown() override;
  void OnPersistenceRestored() override;

  // content::DownloadManager::Observer:
  void OnManagerInitialized() override;
  void ManagerGoingDown(content::DownloadManager* manager) override;
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* item) override;

  // Invoked when the specified `file_path` has completed downloading.
  void OnDownloadCompleted(const base::FilePath& file_path);

  // Removes all observers.
  void RemoveObservers();

  // Callback to invoke when a download is completed.
  ItemDownloadedCallback item_downloaded_callback_;

  // Callback to invoke when all downloads have been restored to holding space.
  DownloadsRestoredCallback downloads_restored_callback_;

  ScopedObserver<content::DownloadManager, content::DownloadManager::Observer>
      download_manager_observer_{this};

  ScopedObserver<download::DownloadItem, download::DownloadItem::Observer>
      download_item_observer_{this};

  base::WeakPtrFactory<HoldingSpaceDownloadsDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_DOWNLOADS_DELEGATE_H_
