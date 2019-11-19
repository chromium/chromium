// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_

#include <memory>
#include <string>

#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

class ChromeAppListItem;

class FakeAppListModelUpdater : public AppListModelUpdater {
 public:
  explicit FakeAppListModelUpdater(Profile* profile = nullptr);
  ~FakeAppListModelUpdater() override;

  // For AppListModel:
  void AddItem(std::unique_ptr<ChromeAppListItem> item) override;
  void AddItemToFolder(std::unique_ptr<ChromeAppListItem> item,
                       const std::string& folder_id) override;
  void AddItemToOemFolder(
      std::unique_ptr<ChromeAppListItem> item,
      app_list::AppListSyncableService::SyncItem* oem_sync_item,
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position) override;
  void UpdateAppItemFromSyncItem(
      app_list::AppListSyncableService::SyncItem* sync_item,
      bool update_name,
      bool update_folder) override;
  void RemoveItem(const std::string& id) override;
  void RemoveUninstalledItem(const std::string& id) override;
  void MoveItemToFolder(const std::string& id,
                        const std::string& folder_id) override;
  // For SearchModel:
  void SetSearchEngineIsGoogle(bool is_google) override;
  void PublishSearchResults(
      const std::vector<ChromeSearchResult*>& results) override;

  void ActivateChromeItem(const std::string& id, int event_flags) override;

  // For AppListModel:
  ChromeAppListItem* FindItem(const std::string& id) override;
  size_t ItemCount() override;
  ChromeAppListItem* ItemAtForTest(size_t index) override;
  ChromeAppListItem* FindFolderItem(const std::string& folder_id) override;
  bool FindItemIndexForTest(const std::string& id, size_t* index) override;
  void GetIdToAppListIndexMap(GetIdToAppListIndexMapCallback callback) override;
  syncer::StringOrdinal GetFirstAvailablePosition() const override;
  void GetContextMenuModel(const std::string& id,
                           GetMenuModelCallback callback) override;
  size_t BadgedItemCount() override;
  // For SearchModel:
  bool SearchEngineIsGoogle() override;
  const std::vector<ChromeSearchResult*>& search_results() const {
    return search_results_;
  }

  void OnFolderCreated(
      std::unique_ptr<ash::AppListItemMetadata> folder) override;
  void OnFolderDeleted(
      std::unique_ptr<ash::AppListItemMetadata> item) override {}
  void OnItemUpdated(std::unique_ptr<ash::AppListItemMetadata> item) override {}
  void OnPageBreakItemAdded(const std::string& id,
                            const syncer::StringOrdinal& position) override {}
  void OnPageBreakItemDeleted(const std::string& id) override {}

  void AddObserver(AppListModelUpdaterObserver* observer) override;
  void RemoveObserver(AppListModelUpdaterObserver* observer) override;

 private:
  bool search_engine_is_google_ = false;
  std::vector<std::unique_ptr<ChromeAppListItem>> items_;
  std::vector<ChromeSearchResult*> search_results_;
  base::ObserverList<AppListModelUpdaterObserver> observers_;
  Profile* profile_;

  void FindOrCreateOemFolder(
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position);
  syncer::StringOrdinal GetOemFolderPos();

  DISALLOW_COPY_AND_ASSIGN(FakeAppListModelUpdater);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
