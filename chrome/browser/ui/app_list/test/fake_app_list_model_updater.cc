// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "extensions/common/constants.h"

FakeAppListModelUpdater::FakeAppListModelUpdater(Profile* profile)
    : profile_(profile) {}

FakeAppListModelUpdater::~FakeAppListModelUpdater() = default;

void FakeAppListModelUpdater::AddItem(std::unique_ptr<ChromeAppListItem> item) {
  items_.push_back(std::move(item));
}

void FakeAppListModelUpdater::AddItemToFolder(
    std::unique_ptr<ChromeAppListItem> item,
    const std::string& folder_id) {
  ChromeAppListItem::TestApi test_api(item.get());
  test_api.SetFolderId(folder_id);
  items_.push_back(std::move(item));
}

void FakeAppListModelUpdater::AddItemToOemFolder(
    std::unique_ptr<ChromeAppListItem> item,
    app_list::AppListSyncableService::SyncItem* oem_sync_item,
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position) {
  syncer::StringOrdinal position_to_try = preferred_oem_position;
  // If we find a valid postion in the sync item, then we'll try it.
  if (oem_sync_item && oem_sync_item->item_ordinal.IsValid())
    position_to_try = oem_sync_item->item_ordinal;
  // In ash:
  FindOrCreateOemFolder(oem_folder_name, position_to_try);
  // In chrome, after oem folder is created:
  AddItemToFolder(std::move(item), ash::kOemFolderId);
}

void FakeAppListModelUpdater::RemoveItem(const std::string& id) {
  size_t index;
  if (FindItemIndexForTest(id, &index))
    items_.erase(items_.begin() + index);
}

void FakeAppListModelUpdater::RemoveUninstalledItem(const std::string& id) {
  RemoveItem(id);
}

void FakeAppListModelUpdater::MoveItemToFolder(const std::string& id,
                                               const std::string& folder_id) {
  size_t index;
  if (FindItemIndexForTest(id, &index)) {
    ChromeAppListItem* item = items_[index].get();
    ChromeAppListItem::TestApi test_api(item);
    test_api.SetFolderId(folder_id);
    for (AppListModelUpdaterObserver& observer : observers_)
      observer.OnAppListItemUpdated(item);
  }
}

void FakeAppListModelUpdater::SetSearchEngineIsGoogle(bool is_google) {
  search_engine_is_google_ = is_google;
}

ChromeAppListItem* FakeAppListModelUpdater::FindItem(const std::string& id) {
  size_t index;
  if (FindItemIndexForTest(id, &index))
    return items_[index].get();
  return nullptr;
}

size_t FakeAppListModelUpdater::ItemCount() {
  return items_.size();
}

ChromeAppListItem* FakeAppListModelUpdater::ItemAtForTest(size_t index) {
  return index < items_.size() ? items_[index].get() : nullptr;
}

bool FakeAppListModelUpdater::FindItemIndexForTest(const std::string& id,
                                                   size_t* index) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i]->id() == id) {
      *index = i;
      return true;
    }
  }
  return false;
}

ChromeAppListItem* FakeAppListModelUpdater::FindFolderItem(
    const std::string& folder_id) {
  ChromeAppListItem* item = FindItem(folder_id);
  return (item && item->is_folder()) ? item : nullptr;
}

void FakeAppListModelUpdater::GetIdToAppListIndexMap(
    GetIdToAppListIndexMapCallback callback) {
  base::flat_map<std::string, uint16_t> id_to_app_list_index;
  for (uint16_t i = 0; i < items_.size(); ++i)
    id_to_app_list_index[items_[i]->id()] = i;
  std::move(callback).Run(id_to_app_list_index);
}

syncer::StringOrdinal FakeAppListModelUpdater::GetFirstAvailablePosition()
    const {
  std::vector<ChromeAppListItem*> top_level_items;
  for (auto& item : items_) {
    DCHECK(item->position().IsValid())
        << "Item with invalid position: id=" << item->id()
        << ", name=" << item->name() << ", is_folder=" << item->is_folder()
        << ", is_page_break=" << item->is_page_break();
    if (item->folder_id().empty() && item->position().IsValid())
      top_level_items.emplace_back(item.get());
  }
  return GetFirstAvailablePositionInternal(top_level_items);
}

void FakeAppListModelUpdater::GetContextMenuModel(
    const std::string& id,
    GetMenuModelCallback callback) {
  std::move(callback).Run(nullptr);
}

void FakeAppListModelUpdater::ActivateChromeItem(const std::string& id,
                                                 int event_flags) {}

size_t FakeAppListModelUpdater::BadgedItemCount() {
  return 0u;
}

bool FakeAppListModelUpdater::SearchEngineIsGoogle() {
  return search_engine_is_google_;
}

void FakeAppListModelUpdater::PublishSearchResults(
    const std::vector<ChromeSearchResult*>& results) {
  search_results_ = results;
}

void FakeAppListModelUpdater::FindOrCreateOemFolder(
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position) {
  ChromeAppListItem* oem_folder = FindFolderItem(ash::kOemFolderId);
  if (oem_folder) {
    std::unique_ptr<ash::AppListItemMetadata> folder_data =
        oem_folder->CloneMetadata();
    folder_data->name = oem_folder_name;
    oem_folder->SetMetadata(std::move(folder_data));
  } else {
    std::unique_ptr<ChromeAppListItem> new_folder =
        std::make_unique<ChromeAppListItem>(nullptr, ash::kOemFolderId,
                                            nullptr);
    oem_folder = new_folder.get();
    std::unique_ptr<ash::AppListItemMetadata> folder_data =
        oem_folder->CloneMetadata();
    folder_data->position = preferred_oem_position.IsValid()
                                ? preferred_oem_position
                                : GetOemFolderPos();
    folder_data->name = oem_folder_name;
    oem_folder->SetMetadata(std::move(folder_data));
    AddItem(std::move(new_folder));
  }
}

syncer::StringOrdinal FakeAppListModelUpdater::GetOemFolderPos() {
  // The oem folder's correct position is based on the item order.
  // We don't have the information in Chrome, so the returned position
  // here is not guaranteed correct.
  size_t web_store_app_index;
  if (!FindItemIndexForTest(extensions::kWebStoreAppId, &web_store_app_index)) {
    if (items_.empty())
      return syncer::StringOrdinal::CreateInitialOrdinal();
    return items_.back()->position().CreateAfter();
  }
  const ChromeAppListItem* web_store_app_item =
      ItemAtForTest(web_store_app_index);
  return web_store_app_item->position().CreateAfter();
}

void FakeAppListModelUpdater::UpdateAppItemFromSyncItem(
    app_list::AppListSyncableService::SyncItem* sync_item,
    bool update_name,
    bool update_folder) {
  // In chrome & ash:
  ChromeAppListItem* chrome_item = FindItem(sync_item->item_id);
  if (!chrome_item)
    return;

  VLOG(2) << this << " UpdateAppItemFromSyncItem: " << sync_item->ToString();
  if (sync_item->item_ordinal.IsValid() &&
      (!chrome_item->position().IsValid() ||
       !chrome_item->position().Equals(sync_item->item_ordinal))) {
    // This updates the position in both chrome and ash:
    chrome_item->SetPosition(sync_item->item_ordinal);
  }
  // Only update the item name if it is a Folder or the name is empty.
  if (update_name && sync_item->item_name != chrome_item->name() &&
      (chrome_item->is_folder() || chrome_item->name().empty())) {
    // This updates the name in both chrome and ash:
    chrome_item->SetName(sync_item->item_name);
  }
  if (update_folder && chrome_item->folder_id() != sync_item->parent_id) {
    VLOG(2) << " Moving Item To Folder: " << sync_item->parent_id;
    // This updates the folder in both chrome and ash:
    MoveItemToFolder(chrome_item->id(), sync_item->parent_id);
  }
}

void FakeAppListModelUpdater::OnFolderCreated(
    std::unique_ptr<ash::AppListItemMetadata> folder) {
  std::unique_ptr<ChromeAppListItem> stub_folder =
      std::make_unique<ChromeAppListItem>(profile_, folder->id, this);

  for (AppListModelUpdaterObserver& observer : observers_)
    observer.OnAppListItemAdded(stub_folder.get());

  AddItem(std::move(stub_folder));
}

void FakeAppListModelUpdater::AddObserver(
    AppListModelUpdaterObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeAppListModelUpdater::RemoveObserver(
    AppListModelUpdaterObserver* observer) {
  observers_.RemoveObserver(observer);
}
