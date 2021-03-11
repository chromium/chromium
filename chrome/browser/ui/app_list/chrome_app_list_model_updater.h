// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"

namespace ash {
class AppListController;
}  // namespace ash

class ChromeAppListItem;

class ChromeAppListModelUpdater : public AppListModelUpdater {
 public:
  explicit ChromeAppListModelUpdater(Profile* profile);
  ChromeAppListModelUpdater(const ChromeAppListModelUpdater&) = delete;
  ChromeAppListModelUpdater& operator=(const ChromeAppListModelUpdater&) =
      delete;
  ~ChromeAppListModelUpdater() override;

  void SetActive(bool active) override;

  // AppListModelUpdater:
  void AddItem(std::unique_ptr<ChromeAppListItem> app_item) override;
  void AddItemToFolder(std::unique_ptr<ChromeAppListItem> app_item,
                       const std::string& folder_id) override;
  void RemoveItem(const std::string& id) override;
  void RemoveUninstalledItem(const std::string& id) override;
  void MoveItemToFolder(const std::string& id,
                        const std::string& folder_id) override;
  void SetStatus(ash::AppListModelStatus status) override;
  void SetSearchEngineIsGoogle(bool is_google) override;
  void UpdateSearchBox(const std::u16string& text,
                       bool initiated_by_user) override;
  void PublishSearchResults(
      const std::vector<ChromeSearchResult*>& results) override;
  std::vector<ChromeSearchResult*> GetPublishedSearchResultsForTest() override;

  // Methods only used by ChromeAppListItem that talk to ash directly.
  void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) override;
  void SetItemName(const std::string& id, const std::string& name) override;
  void SetItemNameAndShortName(const std::string& id,
                               const std::string& name,
                               const std::string& short_name) override;
  void SetAppStatus(const std::string& id, ash::AppStatus app_status) override;
  void SetItemPosition(const std::string& id,
                       const syncer::StringOrdinal& new_position) override;
  void SetItemIsPersistent(const std::string& id, bool is_persistent) override;
  void SetItemFolderId(const std::string& id,
                       const std::string& folder_id) override;
  void SetNotificationBadgeColor(const std::string& id,
                                 const SkColor color) override;

  // Methods only used by ChromeSearchResult that talk to ash directly.
  void SetSearchResultMetadata(
      const std::string& id,
      std::unique_ptr<ash::SearchResultMetadata> metadata) override;

  // Methods only for visiting Chrome items that never talk to ash.
  void ActivateChromeItem(const std::string& id, int event_flags) override;
  ChromeAppListItem* AddChromeItem(std::unique_ptr<ChromeAppListItem> app_item);
  void RemoveChromeItem(const std::string& id);
  void MoveChromeItemToFolder(const std::string& id,
                              const std::string& folder_id);

  // Methods for item querying.
  ChromeAppListItem* FindItem(const std::string& id) override;
  size_t ItemCount() override;
  ChromeAppListItem* ItemAtForTest(size_t index) override;
  ChromeAppListItem* FindFolderItem(const std::string& folder_id) override;
  bool FindItemIndexForTest(const std::string& id, size_t* index) override;
  bool SearchEngineIsGoogle() override;
  void GetIdToAppListIndexMap(GetIdToAppListIndexMapCallback callback) override;
  size_t BadgedItemCount() override;
  void GetContextMenuModel(const std::string& id,
                           GetMenuModelCallback callback) override;
  syncer::StringOrdinal GetFirstAvailablePosition() const override;
  syncer::StringOrdinal GetPositionBeforeFirstItem() const override;

  // Methods for AppListSyncableService:
  void AddItemToOemFolder(
      std::unique_ptr<ChromeAppListItem> item,
      app_list::AppListSyncableService::SyncItem* oem_sync_item,
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position) override;
  void ResolveOemFolderPosition(
      const syncer::StringOrdinal& preferred_oem_position,
      ResolveOemFolderPositionCallback callback) override;
  void UpdateAppItemFromSyncItem(
      app_list::AppListSyncableService::SyncItem* sync_item,
      bool update_name,
      bool update_folder) override;
  void NotifyProcessSyncChangesFinished() override;

  // Methods to handle model update from ash:
  void OnItemAdded(std::unique_ptr<ash::AppListItemMetadata> item) override;
  void OnItemUpdated(std::unique_ptr<ash::AppListItemMetadata> item) override;
  void OnFolderDeleted(std::unique_ptr<ash::AppListItemMetadata> item) override;
  void OnPageBreakItemDeleted(const std::string& id) override;

  void AddObserver(AppListModelUpdaterObserver* observer) override;
  void RemoveObserver(AppListModelUpdaterObserver* observer) override;

 private:
  std::vector<ChromeAppListItem*> GetTopLevelItems() const;

  // A map from a ChromeAppListItem's id to its unique pointer. This item set
  // matches the one in AppListModel.
  std::map<std::string, std::unique_ptr<ChromeAppListItem>> items_;
  // The most recently list of search results.
  std::vector<ChromeSearchResult*> published_results_;
  Profile* const profile_ = nullptr;
  base::ObserverList<AppListModelUpdaterObserver> observers_;
  ash::AppListController* app_list_controller_ = nullptr;
  bool search_engine_is_google_ = false;

  base::WeakPtrFactory<ChromeAppListModelUpdater> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_
