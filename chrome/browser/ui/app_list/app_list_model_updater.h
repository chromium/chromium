// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/app_list/app_list_model_updater_observer.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

class ChromeAppListItem;
class ChromeSearchResult;

namespace ui {
class SimpleMenuModel;
}  // namespace ui

// An interface to wrap AppListModel access in browser.
class AppListModelUpdater {
 public:
  class TestApi {
   public:
    explicit TestApi(AppListModelUpdater* model_updater)
        : model_updater_(model_updater) {}
    ~TestApi() = default;

    void SetItemPosition(const std::string& id,
                         const syncer::StringOrdinal& new_position) {
      model_updater_->SetItemPosition(id, new_position);
    }

   private:
    AppListModelUpdater* const model_updater_;
  };

  virtual ~AppListModelUpdater() {}

  int model_id() const { return model_id_; }

  // Set whether this model updater is active.
  // When we have multiple user profiles, only the active one has access to the
  // model. All others profile can only cache model changes in Chrome.
  virtual void SetActive(bool active) {}

  // For AppListModel:
  virtual void AddItem(std::unique_ptr<ChromeAppListItem> item) {}
  virtual void AddItemToFolder(std::unique_ptr<ChromeAppListItem> item,
                               const std::string& folder_id) {}
  virtual void RemoveItem(const std::string& id) {}
  virtual void RemoveUninstalledItem(const std::string& id) {}
  virtual void MoveItemToFolder(const std::string& id,
                                const std::string& folder_id) {}
  virtual void SetStatus(ash::AppListModelStatus status) {}
  virtual void SetState(ash::AppListState state) {}
  virtual void HighlightItemInstalledFromUI(const std::string& id) {}
  // For SearchModel:
  virtual void SetSearchEngineIsGoogle(bool is_google) {}
  virtual void SetSearchTabletAndClamshellAccessibleName(
      const base::string16& tablet_accessible_name,
      const base::string16& clamshell_accessible_name) {}
  virtual void SetSearchHintText(const base::string16& hint_text) {}
  virtual void UpdateSearchBox(const base::string16& text,
                               bool initiated_by_user) {}
  virtual void PublishSearchResults(
      const std::vector<ChromeSearchResult*>& results) {}

  // Item field setters only used by ChromeAppListItem and its derived classes.
  virtual void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) {}
  virtual void SetItemName(const std::string& id, const std::string& name) {}
  virtual void SetItemNameAndShortName(const std::string& id,
                                       const std::string& name,
                                       const std::string& short_name) {}
  virtual void SetItemPosition(const std::string& id,
                               const syncer::StringOrdinal& new_position) {}
  virtual void SetItemIsPersistent(const std::string& id, bool is_persistent) {}
  virtual void SetItemFolderId(const std::string& id,
                               const std::string& folder_id) {}
  virtual void SetItemIsInstalling(const std::string& id, bool is_installing) {}
  virtual void SetItemPercentDownloaded(const std::string& id,
                                        int32_t percent_downloaded) {}

  virtual void SetSearchResultMetadata(
      const std::string& id,
      std::unique_ptr<ash::SearchResultMetadata> metadata) {}
  virtual void SetSearchResultIsInstalling(const std::string& id,
                                           bool is_installing) {}
  virtual void SetSearchResultPercentDownloaded(const std::string& id,
                                                int percent_downloaded) {}
  virtual void SetSearchResultIcon(const std::string& id,
                                   const gfx::ImageSkia& icon) {}
  virtual void SetSearchResultBadgeIcon(const std::string& id,
                                        const gfx::ImageSkia& badge_icon) {}
  virtual void NotifySearchResultItemInstalled(const std::string& id) {}
  virtual void ActivateChromeItem(const std::string& id, int event_flags) {}

  // For AppListModel:
  virtual ChromeAppListItem* FindItem(const std::string& id) = 0;
  virtual size_t ItemCount() = 0;
  virtual ChromeAppListItem* ItemAtForTest(size_t index) = 0;
  virtual ChromeAppListItem* FindFolderItem(const std::string& folder_id) = 0;
  virtual bool FindItemIndexForTest(const std::string& id, size_t* index) = 0;
  using GetIdToAppListIndexMapCallback =
      base::OnceCallback<void(const base::flat_map<std::string, uint16_t>&)>;
  virtual void GetIdToAppListIndexMap(GetIdToAppListIndexMapCallback callback) {
  }
  virtual syncer::StringOrdinal GetFirstAvailablePosition() const = 0;

  // Methods for AppListSyncableService:
  virtual void AddItemToOemFolder(
      std::unique_ptr<ChromeAppListItem> item,
      app_list::AppListSyncableService::SyncItem* oem_sync_item,
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position) {}
  using ResolveOemFolderPositionCallback =
      base::OnceCallback<void(ChromeAppListItem*)>;
  virtual void ResolveOemFolderPosition(
      const syncer::StringOrdinal& preferred_oem_position,
      ResolveOemFolderPositionCallback callback) {}
  virtual void UpdateAppItemFromSyncItem(
      app_list::AppListSyncableService::SyncItem* sync_item,
      bool update_name,
      bool update_folder) {}

  using GetMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetContextMenuModel(const std::string& id,
                                   GetMenuModelCallback callback) = 0;
  virtual size_t BadgedItemCount() = 0;
  // For SearchModel:
  virtual bool SearchEngineIsGoogle() = 0;

  // Methods for handle model updates in ash:
  virtual void OnFolderCreated(
      std::unique_ptr<ash::AppListItemMetadata> item) = 0;
  virtual void OnFolderDeleted(
      std::unique_ptr<ash::AppListItemMetadata> item) = 0;
  virtual void OnItemUpdated(
      std::unique_ptr<ash::AppListItemMetadata> item) = 0;
  virtual void OnPageBreakItemAdded(const std::string& id,
                                    const syncer::StringOrdinal& position) = 0;
  virtual void OnPageBreakItemDeleted(const std::string& id) = 0;

  virtual void AddObserver(AppListModelUpdaterObserver* observer) = 0;
  virtual void RemoveObserver(AppListModelUpdaterObserver* observer) = 0;

 protected:
  AppListModelUpdater();

  // Returns the first available position in app list. |top_level_items| are
  // items without parents. Note that all items in |top_level_items| should have
  // valid position.
  static syncer::StringOrdinal GetFirstAvailablePositionInternal(
      const std::vector<ChromeAppListItem*>& top_level_items);

 private:
  const int model_id_;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
