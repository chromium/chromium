// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_

#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"
#include "components/account_id/account_id.h"
#include "components/download/public/common/download_item.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/download_manager.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Browser context keyed service that:
// *   Manages the temporary holding space per-profile data model.
// *   Serves as an entry point to add holding space items from Chrome.
class HoldingSpaceKeyedService : public KeyedService,
                                 public HoldingSpaceModelObserver,
                                 public ProfileManagerObserver,
                                 public content::DownloadManager::Observer,
                                 public download::DownloadItem::Observer {
 public:
  // Preference path at which holding space items are persisted.
  // NOTE: Any changes to persistence must be backwards compatible.
  static constexpr char kPersistencePath[] = "ash.holding_space.items";

  HoldingSpaceKeyedService(content::BrowserContext* context,
                           const AccountId& account_id);
  HoldingSpaceKeyedService(const HoldingSpaceKeyedService& other) = delete;
  HoldingSpaceKeyedService& operator=(const HoldingSpaceKeyedService& other) =
      delete;
  ~HoldingSpaceKeyedService() override;

  // Registers profile preferences for holding space.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Methods to add an item backed by the provided absolute file path.
  // The path is expected to be under a mount point path recognized by the file
  // manager app (otherwise, the item will be dropped silently).
  void AddScreenshot(const base::FilePath& screenshot_path,
                     const gfx::ImageSkia& image);
  void AddDownload(const base::FilePath& download_path);

  void RemoveDownloadManagerObservers();

  // KeyedService:
  void Shutdown() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // content::DownloadManager::Observer implementation:
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadDropped(content::DownloadManager* manager) override;
  void OnManagerInitialized() override;
  void ManagerGoingDown(content::DownloadManager* manager) override;

  // download::DownloadItem::Observer implementation:
  void OnDownloadUpdated(download::DownloadItem* item) override;


  const HoldingSpaceClient* client_for_testing() const {
    return &holding_space_client_;
  }

  const HoldingSpaceModel* model_for_testing() const {
    return &holding_space_model_;
  }
  void SetDownloadManagerForTesting(content::DownloadManager* manager);

 private:
  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;

  // Restores |holding_space_model_| from persistent storage.
  void RestoreModel();

  // Resolves file attributes from a file path;
  GURL ResolveFileSystemUrl(const base::FilePath& file_path) const;
  gfx::ImageSkia ResolveImage(const base::FilePath& file_path) const;

  content::BrowserContext* const browser_context_;
  HoldingSpaceClientImpl holding_space_client_;
  HoldingSpaceModel holding_space_model_;

  ScopedObserver<HoldingSpaceModel, HoldingSpaceModelObserver>
      holding_space_model_observer_{this};

  content::DownloadManager* download_manager_ = nullptr;
  ScopedObserver<ProfileManager, ProfileManagerObserver>
      observed_profile_manager_{this};
  ScopedObserver<download::DownloadItem, download::DownloadItem::Observer>
      download_items_observer_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
