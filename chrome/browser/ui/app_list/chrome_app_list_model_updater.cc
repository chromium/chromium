// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"

#include <unordered_map>
#include <utility>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item_manager.h"
#include "chrome/browser/ui/app_list/reorder/app_list_reorder_delegate.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "extensions/common/constants.h"
#include "ui/base/models/menu_model.h"

namespace {

std::unique_ptr<ash::AppListItem> CreateAppListItem(
    std::unique_ptr<ash::AppListItemMetadata> metadata,
    ash::AppListModelDelegate* delegate) {
  std::unique_ptr<ash::AppListItem> app_list_item =
      metadata->is_folder
          ? std::make_unique<ash::AppListFolderItem>(metadata->id, delegate)
          : std::make_unique<ash::AppListItem>(metadata->id);
  app_list_item->SetMetadata(std::move(metadata));
  return app_list_item;
}

}  // namespace

// TemporarySortManager --------------------------------------------------------

// A helper class for managing temporary sort data. Used only when the app list
// is under temporary sort.
class ChromeAppListModelUpdater::TemporarySortManager {
 public:
  TemporarySortManager(
      ash::AppListSortOrder temporary_order,
      const std::map<std::string, std::unique_ptr<ChromeAppListItem>>&
          permanent_items)
      : temporary_order_(temporary_order) {
    DCHECK(ash::features::IsLauncherAppSortEnabled());

    // Fill permanent position storage.
    for (const auto& id_item_pair : permanent_items) {
      const std::string& id = id_item_pair.first;
      DCHECK(!HasId(id));
      permanent_position_storage_.emplace(id, id_item_pair.second->position());
    }
  }
  TemporarySortManager(const TemporarySortManager&) = delete;
  TemporarySortManager& operator=(const TemporarySortManager&) = delete;
  ~TemporarySortManager() = default;

  bool HasId(const std::string& id) const {
    return permanent_position_storage_.find(id) !=
           permanent_position_storage_.cend();
  }

  const syncer::StringOrdinal& GetPermanentPositionForId(
      const std::string& id) const {
    auto iter = permanent_position_storage_.find(id);
    DCHECK(iter != permanent_position_storage_.cend());
    return iter->second;
  }

  void Deactivate() {
    DCHECK(is_active_);
    is_active_ = false;
  }

  ash::AppListSortOrder temporary_order() const { return temporary_order_; }
  void set_temporary_order(ash::AppListSortOrder new_order) {
    temporary_order_ = new_order;
  }

  bool is_active() const { return is_active_; }

 private:
  // Indicates the app list order that is not committed yet. When under
  // temporary sort, the app list items on the local device (i.e. the device on
  // which `temporary_order` is triggered) are sorted with `temporary_order_`.
  // But the new positions are not synced with other devices.
  ash::AppListSortOrder temporary_order_ = ash::AppListSortOrder::kCustom;

  // For each key-value pair, the key is an item id while the value is the
  // permanent position (i.e. the position that is shared with other synced
  // devices) of the item indexed by the key.
  std::map<std::string, syncer::StringOrdinal> permanent_position_storage_;

  // `TemporarySortManager` is active when:
  // (1) App list is under temporary sort, and
  // (2) App list is not in the progress of ending temporary sort.
  // Note that model updates will not be propagated to observers until the
  // temporary sort manager is deactivated
  bool is_active_ = true;
};

// ChromeAppListModelUpdater ---------------------------------------------------

ChromeAppListModelUpdater::ChromeAppListModelUpdater(
    Profile* profile,
    app_list::AppListReorderDelegate* order_delegate)
    : profile_(profile),
      order_delegate_(order_delegate),
      item_manager_(std::make_unique<ChromeAppListItemManager>()),
      model_(this) {
  DCHECK_EQ(ash::features::IsLauncherAppSortEnabled(),
            static_cast<bool>(order_delegate_));
  model_.AddObserver(this);
}

ChromeAppListModelUpdater::~ChromeAppListModelUpdater() {
  model_.RemoveObserver(this);
}

void ChromeAppListModelUpdater::SetActive(bool active) {
  is_active_ = active;

  if (active) {
    ash::AppListController::Get()->SetActiveModel(model_id(), &model_,
                                                  &search_model_);
  }
}

void ChromeAppListModelUpdater::AddItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  std::unique_ptr<ash::AppListItemMetadata> item_data =
      app_item->CloneMetadata();

  // With ProductivityLauncher, ignore page break items because empty slots
  // only exist on the last launcher page. Therefore syncing on page break items
  // is unnecessary.
  if (item_data->is_page_break &&
      ash::features::IsProductivityLauncherEnabled()) {
    return;
  }

  // Add to Chrome first leave all updates to observer methods.
  item_manager_->AddChromeItem(std::move(app_item));
  const std::string folder_id = item_data->folder_id;
  item_data->folder_id.clear();
  if (folder_id.empty()) {
    model_.AddItem(CreateAppListItem(std::move(item_data), this));
  } else {
    model_.AddItemToFolder(CreateAppListItem(std::move(item_data), this),
                           folder_id);
  }
}

void ChromeAppListModelUpdater::AddItemToFolder(
    std::unique_ptr<ChromeAppListItem> app_item,
    const std::string& folder_id) {
  std::unique_ptr<ash::AppListItemMetadata> item_data =
      app_item->CloneMetadata();
  // Add to Chrome first leave all updates to observer methods.
  // TODO(https://crbug.com/1261796): now we are using the APIs provided by
  // `ChromeAppListItem` to set item attributes. It is buggy-prone because
  // when an item's attribute is updated in this way, `item_manager_` is not
  // aware of the update. We should ensure that after an item is added to
  // `item_manager_` the item is always updated through `item_manager_`.
  app_item->SetChromeFolderId(folder_id);
  ChromeAppListItem* item_added =
      item_manager_->AddChromeItem(std::move(app_item));

  item_data->folder_id.clear();
  model_.AddItemToFolder(CreateAppListItem(std::move(item_data), this),
                         folder_id);
  // Set the item's default icon if it has one.
  if (!item_added->icon().isNull()) {
    ash::AppListItem* item = model_.FindItem(item_added->id());
    item->SetDefaultIcon(item_added->icon());
  }
}

void ChromeAppListModelUpdater::RemoveItem(const std::string& id) {
  // Copy the ID to the stack since it may to be destroyed in
  // RemoveChromeItem(). See crbug.com/1190347.
  std::string id_copy = id;
  item_manager_->RemoveChromeItem(id_copy);
  model_.DeleteItem(id_copy);
}

void ChromeAppListModelUpdater::RemoveUninstalledItem(const std::string& id) {
  // Copy the ID to the stack since it may to be destroyed in
  // RemoveChromeItem(). See crbug.com/1190347.
  std::string id_copy = id;
  item_manager_->RemoveChromeItem(id_copy);
  model_.DeleteUninstalledItem(id_copy);
}

void ChromeAppListModelUpdater::SetStatus(ash::AppListModelStatus status) {
  model_.SetStatus(status);
}

void ChromeAppListModelUpdater::SetSearchEngineIsGoogle(bool is_google) {
  search_engine_is_google_ = is_google;
  search_model_.SetSearchEngineIsGoogle(is_google);
}

void ChromeAppListModelUpdater::UpdateSearchBox(const std::u16string& text,
                                                bool initiated_by_user) {
  search_model_.search_box()->Update(text, initiated_by_user);
}

void ChromeAppListModelUpdater::PublishSearchResults(
    const std::vector<ChromeSearchResult*>& results,
    const std::vector<ash::AppListSearchResultCategory>& categories) {
  published_results_ = results;

  for (auto* const result : results)
    result->set_model_updater(this);

  std::vector<std::unique_ptr<ash::SearchResult>> ash_results;
  std::vector<std::unique_ptr<ash::SearchResultMetadata>> result_data;
  for (auto* result : results) {
    auto ash_result = std::make_unique<ash::SearchResult>();
    ash_result->SetMetadata(result->CloneMetadata());
    ash_results.push_back(std::move(ash_result));
  }
  search_model_.PublishResults(std::move(ash_results), categories);
}

std::vector<ChromeSearchResult*>
ChromeAppListModelUpdater::GetPublishedSearchResultsForTest() {
  return published_results_;
}

void ChromeAppListModelUpdater::ActivateChromeItem(const std::string& id,
                                                   int event_flags) {
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  DCHECK(!item->is_folder());
  item->PerformActivate(event_flags);
}

void ChromeAppListModelUpdater::LoadAppIcon(const std::string& id) {
  ChromeAppListItem* item = FindItem(id);
  if (!item)
    return;
  item->LoadIcon();
}

////////////////////////////////////////////////////////////////////////////////
// Methods only used by ChromeAppListItem that talk to ash directly.

void ChromeAppListModelUpdater::SetItemIconVersion(const std::string& id,
                                                   int icon_version) {
  ash::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetIconVersion(icon_version);
}

void ChromeAppListModelUpdater::SetItemIcon(const std::string& id,
                                            const gfx::ImageSkia& icon) {
  ash::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetDefaultIcon(icon);
}

void ChromeAppListModelUpdater::SetItemName(const std::string& id,
                                            const std::string& name) {
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->name = name;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemNameAndShortName(
    const std::string& id,
    const std::string& name,
    const std::string& short_name) {
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->name = name;
  data->short_name = short_name;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetAppStatus(const std::string& id,
                                             ash::AppStatus app_status) {
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->app_status = app_status;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemPosition(
    const std::string& id,
    const syncer::StringOrdinal& new_position) {
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  DCHECK(new_position.IsValid());
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->position = new_position;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemIsPersistent(const std::string& id,
                                                    bool is_persistent) {
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->is_persistent = is_persistent;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetItemFolderId(const std::string& id,
                                                const std::string& folder_id) {
  ash::AppListItem* item = model_.FindItem(id);
  if (!item)
    return;
  std::unique_ptr<ash::AppListItemMetadata> data = item->CloneMetadata();
  data->folder_id = folder_id;
  model_.SetItemMetadata(id, std::move(data));
}

void ChromeAppListModelUpdater::SetNotificationBadgeColor(const std::string& id,
                                                          const SkColor color) {
  ash::AppListItem* item = model_.FindItem(id);
  if (item)
    item->SetNotificationBadgeColor(color);
}

////////////////////////////////////////////////////////////////////////////////
// Methods only used by ChromeSearchResult that talk to ash directly.

void ChromeAppListModelUpdater::SetSearchResultMetadata(
    const std::string& id,
    std::unique_ptr<ash::SearchResultMetadata> metadata) {
  ash::SearchResult* result = search_model_.FindSearchResult(metadata->id);
  if (result)
    result->SetMetadata(std::move(metadata));
}

////////////////////////////////////////////////////////////////////////////////
// Methods for item querying

ChromeAppListItem* ChromeAppListModelUpdater::FindItem(const std::string& id) {
  return item_manager_->FindItem(id);
}

size_t ChromeAppListModelUpdater::ItemCount() {
  return item_manager_->ItemCount();
}

std::vector<const ChromeAppListItem*> ChromeAppListModelUpdater::GetItems()
    const {
  std::vector<const ChromeAppListItem*> item_pointers;
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();
  for (auto& entry : items)
    item_pointers.push_back(entry.second.get());
  return item_pointers;
}

std::vector<ChromeAppListItem*> ChromeAppListModelUpdater::GetTopLevelItems()
    const {
  return item_manager_->GetTopLevelItems();
}

ChromeAppListItem* ChromeAppListModelUpdater::ItemAtForTest(size_t index) {
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();
  DCHECK_LT(index, items.size());
  DCHECK_LE(0u, index);
  auto it = items.cbegin();
  for (size_t i = 0; i < index; ++i)
    ++it;
  return it->second.get();
}

ChromeAppListItem* ChromeAppListModelUpdater::FindFolderItem(
    const std::string& folder_id) {
  ChromeAppListItem* item = FindItem(folder_id);
  return (item && item->is_folder()) ? item : nullptr;
}

bool ChromeAppListModelUpdater::FindItemIndexForTest(const std::string& id,
                                                     size_t* index) {
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();

  *index = 0;
  for (auto it = items.begin(); it != items.end(); ++it) {
    if (it->second->id() == id)
      return true;
    ++(*index);
  }
  return false;
}

bool ChromeAppListModelUpdater::SearchEngineIsGoogle() {
  return search_engine_is_google_;
}

void ChromeAppListModelUpdater::GetIdToAppListIndexMap(
    GetIdToAppListIndexMapCallback callback) {
  base::flat_map<std::string, uint16_t> id_to_app_list_index;
  for (size_t i = 0; i < model_.top_level_item_list()->item_count(); ++i) {
    id_to_app_list_index[model_.top_level_item_list()->item_at(i)->id()] = i;
  }
  std::move(callback).Run(id_to_app_list_index);
}

size_t ChromeAppListModelUpdater::BadgedItemCount() {
  return item_manager_->BadgedItemCount();
}

void ChromeAppListModelUpdater::GetContextMenuModel(
    const std::string& id,
    GetMenuModelCallback callback) {
  ChromeAppListItem* item = FindItem(id);
  // TODO(stevenjb/jennyz): Implement this for folder items.
  // TODO(newcomer): Add histograms for folder items.
  if (!item || item->is_folder()) {
    std::move(callback).Run(nullptr);
    return;
  }
  item->GetContextMenuModel(std::move(callback));
}

syncer::StringOrdinal ChromeAppListModelUpdater::CalculatePositionForNewItem(
    const ChromeAppListItem& new_item) {
  // TODO(https://crbug.com/1260875): handle the case that `new_item` is a
  // folder.
  if (!ash::features::IsLauncherAppSortEnabled() || new_item.is_folder())
    return GetFirstAvailablePosition();

  // TODO(https://crbug.com/1260877): ideally we would not have to create a
  // one-off vector of items using `GetItems()`.
  return order_delegate_->CalculatePositionForNewItem(new_item, GetItems());
}

syncer::StringOrdinal ChromeAppListModelUpdater::GetPositionBeforeFirstItem()
    const {
  return GetPositionBeforeFirstItemInternal(GetTopLevelItems());
}

////////////////////////////////////////////////////////////////////////////////
// Methods for AppListSyncableService

void ChromeAppListModelUpdater::UpdateAppItemFromSyncItem(
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
    SetItemPosition(chrome_item->id(), sync_item->item_ordinal);
  }
  // Only update the item name if it is a Folder or the name is empty.
  if (update_name && sync_item->item_name != chrome_item->name() &&
      (chrome_item->is_folder() || chrome_item->name().empty())) {
    // This updates the name in both chrome and ash:
    SetItemName(chrome_item->id(), sync_item->item_name);
  }
  if (update_folder && chrome_item->folder_id() != sync_item->parent_id) {
    VLOG(2) << " Moving Item To Folder: " << sync_item->parent_id;
    // This updates the folder in both chrome and ash:
    SetItemFolderId(chrome_item->id(), sync_item->parent_id);
  }
}

void ChromeAppListModelUpdater::NotifyProcessSyncChangesFinished() {
  if (is_active_) {
    AppListClientImpl::GetInstance()
        ->GetAppListController()
        ->NotifyProcessSyncChangesFinished();
  }
}

void ChromeAppListModelUpdater::AddObserver(
    AppListModelUpdaterObserver* observer) {
  observers_.AddObserver(observer);
}

void ChromeAppListModelUpdater::RemoveObserver(
    AppListModelUpdaterObserver* observer) {
  observers_.RemoveObserver(observer);
}

ash::AppListSortOrder ChromeAppListModelUpdater::GetTemporarySortOrderForTest()
    const {
  return temporary_sort_manager_->temporary_order();
}

////////////////////////////////////////////////////////////////////////////////
// Methods called from Ash:

void ChromeAppListModelUpdater::OnAppListItemAdded(ash::AppListItem* item) {
  ChromeAppListItem* chrome_item = FindItem(item->id());
  // If the item already exists, we should have set its information properly.
  if (!chrome_item) {
    // Otherwise, we detect an item is created in Ash which is not added into
    // our Chrome list yet. This only happens when a folder is created or when a
    // page break is added.
    DCHECK(item->is_folder() || item->is_page_break());
    std::unique_ptr<ChromeAppListItem> new_item =
        std::make_unique<ChromeAppListItem>(profile_, item->id(), this);
    new_item->SetMetadata(item->CloneMetadata());
    chrome_item = item_manager_->AddChromeItem(std::move(new_item));
  }

  // Do not propagate the addition of page break items from Ash side to remote
  // side if ProductivityLauncher feature is enabled. Because:
  // (1) If a remote device enables ProductivityLauncher as well, it will
  // generate a page break item by its own when the current launcher page has no
  // space for extra icons. In other words, it does not need to sync on page
  // break items with other devices.
  // (2) If a remote device disables the feature flag, syncing on page break
  // items with those with the flag enabled does not bring the consistent
  // launcher layout.
  // TODO(crbug.com/1233729): Simply stopping the syncs on page break items may
  // lead to overflow pages on the device with the feature flag disabled.
  // Therefore we should handle the page break item sync in a better way.
  // TODO(crbug.com/1234588): Ideally we should not send page breaks from/to the
  // app list controller if the feature to remove spaces is enabled.
  if (chrome_item->is_page_break() &&
      ash::features::IsProductivityLauncherEnabled()) {
    return;
  }

  // Notify observers that an item is added to the AppListModel in ash.
  // Note that items of apps are added from Chrome side so there would be an
  // existing |chrome_item| when running here.
  MaybeNotifyObserversOfItemChange(chrome_item, ItemChangeType::kAdd);
}

void ChromeAppListModelUpdater::OnAppListItemUpdated(ash::AppListItem* item) {
  ChromeAppListItem* chrome_item = FindItem(item->id());

  // Ignore the item if it does not exist. This happens when a race occurs
  // between the browser and ash. e.g. An item is removed on browser side while
  // there is an in-flight OnItemUpdated() call from ash.
  if (!chrome_item)
    return;

  // Preserve icon once it cannot be modified at ash.
  item->SetDefaultIcon(chrome_item->icon());

  const std::string copy_id = item->id();
  item_manager_->UpdateChromeItem(copy_id, item->CloneMetadata());
  MaybeNotifyObserversOfItemChange(chrome_item, ItemChangeType::kUpdate);
}

void ChromeAppListModelUpdater::OnAppListItemWillBeDeleted(
    ash::AppListItem* item) {
  if (!item->is_folder() && !item->is_page_break())
    return;

  ChromeAppListItem* chrome_item = FindItem(item->id());
  if (!chrome_item) {
    LOG(ERROR) << "OnAppListItemWillBeDeleted: " << item->id()
               << " does not exist.";
    return;
  }

  MaybeNotifyObserversOfItemChange(chrome_item, ItemChangeType::kDelete);
  item_manager_->RemoveChromeItem(item->id());
}

void ChromeAppListModelUpdater::OnSortRequested(ash::AppListSortOrder order) {
  CHECK_NE(ash::AppListSortOrder::kCustom, order);

  // Ignore sort requests if sorting makes no visual difference.
  if (item_manager_->ItemCount() < 2)
    return;

  if (is_under_temporary_sort()) {
    // Sorting can be triggered when app list is under temporary sort.
    if (temporary_sort_manager_->temporary_order() == order) {
      // Order does not change so nothing to do.
      return;
    }

    // Permanent positions are already stored. Therefore only update
    // the temporary order here.
    temporary_sort_manager_->set_temporary_order(order);
  } else {
    // The app list was not under temporary sort - initialize it now.
    temporary_sort_manager_ =
        std::make_unique<TemporarySortManager>(order, item_manager_->items());
  }

  // The code below changes the item positions based on the new order.

  // Calculate item positions under temporary sort order.
  std::vector<app_list::reorder::ReorderParam> reorder_params =
      order_delegate_->GenerateReorderParamsForAppListItems(order, GetItems());

  // Notify the ash side of the new positions. Updates are local-only because
  // `temporary_sort_manager_` is active.
  for (const auto& reorder_param : reorder_params)
    SetItemPosition(reorder_param.sync_item_id, reorder_param.ordinal);
}

void ChromeAppListModelUpdater::OnSortRevertRequested() {
  if (!temporary_sort_manager_)
    return;

  EndTemporarySortAndTakeAction(EndAction::kRevert);
}

void ChromeAppListModelUpdater::RequestPositionUpdate(
    std::string id,
    const syncer::StringOrdinal& new_position,
    ash::RequestPositionUpdateReason reason) {
  DCHECK(FindItem(id));
  SetItemPosition(id, new_position);

  // Return early if there is no uncommitted sort orders.
  if (!temporary_sort_manager_)
    return;

  // Clear the sort order if an item is moved by user manually.
  if (reason == ash::RequestPositionUpdateReason::kMoveItem)
    EndTemporarySortAndTakeAction(EndAction::kCommitAndClearSort);
}

void ChromeAppListModelUpdater::RequestMoveItemToFolder(
    std::string id,
    const std::string& folder_id) {
  DCHECK(!folder_id.empty());

  // The target position relies on the items under the target folder. Therefore
  // calculate `target_position` before moving the item to the folder.
  syncer::StringOrdinal target_position;
  ChromeAppListItem* last_child =
      item_manager_->FindLastChildInFolder(folder_id);
  if (!last_child) {
    // The moved item is the first item under folder.
    target_position = syncer::StringOrdinal::CreateInitialOrdinal();
  } else {
    // TODO(https://crbug.com/1247408): now the new item is always added to the
    // rear. We should take launcher sort order into consideration.
    target_position = last_child->position().CreateAfter();
  }

  SetItemFolderId(id, folder_id);
  SetItemPosition(id, target_position);
}

void ChromeAppListModelUpdater::RequestMoveItemToRoot(
    std::string id,
    syncer::StringOrdinal target_position) {
  SetItemFolderId(id, "");

  SetItemPosition(id, target_position);
}

// Private methods -------------------------------------------------------------

void ChromeAppListModelUpdater::MaybeNotifyObserversOfItemChange(
    ChromeAppListItem* chrome_item,
    ItemChangeType type) {
  // If `temporary_sort_manager_` is active, item changes are not propagated to
  // observers.
  if (is_under_temporary_sort() && temporary_sort_manager_->is_active())
    return;

  switch (type) {
    case ItemChangeType::kAdd:
      for (AppListModelUpdaterObserver& observer : observers_)
        observer.OnAppListItemAdded(chrome_item);
      break;
    case ItemChangeType::kUpdate:
      for (AppListModelUpdaterObserver& observer : observers_)
        observer.OnAppListItemUpdated(chrome_item);
      break;
    case ItemChangeType::kDelete:
      for (AppListModelUpdaterObserver& observer : observers_)
        observer.OnAppListItemWillBeDeleted(chrome_item);
      break;
  }
}

void ChromeAppListModelUpdater::EndTemporarySortAndTakeAction(
    EndAction action) {
  DCHECK(is_under_temporary_sort() && temporary_sort_manager_->is_active());

  // Allow item updates to be propagated to observers.
  temporary_sort_manager_->Deactivate();

  switch (action) {
    case EndAction::kCommit:
      CommitTemporaryPositions();
      CommitOrder();
      break;
    case EndAction::kRevert:
      RevertTemporaryPositions();
      break;
    case EndAction::kCommitAndClearSort:
      CommitTemporaryPositions();
      ClearOrder();
      break;
  }

  temporary_sort_manager_.reset();
}

void ChromeAppListModelUpdater::RevertTemporaryPositions() {
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();
  for (const auto& id_item_pair : items) {
    const std::string& id = id_item_pair.first;

    // TODO(https://crbug.com/1260447): the features of temporary sort are
    // partially implemented. The cases of app installation/removal are not
    // handled right now. As a result, the ids in `temporary_sort_manager_`
    // can be inconsistent with those in `item_manager_`. Therefore manually
    // check the existence of `id` here. When the issue gets addressed, replace
    // with a DCHECK statement.
    if (!temporary_sort_manager_->HasId(id))
      continue;

    const syncer::StringOrdinal& temporary_position =
        id_item_pair.second->position();
    const syncer::StringOrdinal& permanent_position =
        temporary_sort_manager_->GetPermanentPositionForId(id);

    if (temporary_position.IsValid() && permanent_position.IsValid() &&
        temporary_position.Equals(permanent_position)) {
      // Skip if `temporary_position` is equal to `permanent_position`.
      continue;
    }

    // Set the local item with permanent position.
    SetItemPosition(id, permanent_position);
  }
}

void ChromeAppListModelUpdater::CommitTemporaryPositions() {
  const std::map<std::string, std::unique_ptr<ChromeAppListItem>>& items =
      item_manager_->items();
  for (const auto& id_item_pair : items) {
    const syncer::StringOrdinal& temporary_position =
        id_item_pair.second->position();

    if (!temporary_position.IsValid()) {
      // Not sure whether this branch can be executed. Handle this case for
      // safety. TODO(https://crbug.com/1263795): check whether the positions
      // stored in `item_manager_` are always valid. If so, remove this code.
      continue;
    }

    // TODO(https://crbug.com/1260447): the features of temporary sort are
    // partially implemented. The cases of app installation/removal are not
    // handled right now. As a result, the ids in `temporary_sort_manager_`
    // can be inconsistent with those in `item_manager_`. Therefore manually
    // check the existence of `id` here. When the issue gets addressed, replace
    // with a DCHECK statement.
    const std::string& id = id_item_pair.first;
    if (!temporary_sort_manager_->HasId(id))
      continue;

    const syncer::StringOrdinal& permanent_position =
        temporary_sort_manager_->GetPermanentPositionForId(id);
    if (permanent_position.IsValid() &&
        permanent_position.Equals(temporary_position)) {
      // The temporary sort order does not modify the item's position so no
      // work to do for this item.
      continue;
    }

    // Propagate the item position update.
    MaybeNotifyObserversOfItemChange(id_item_pair.second.get(),
                                     ItemChangeType::kUpdate);
  }
}

void ChromeAppListModelUpdater::CommitOrder() {
  // TODO(https://crbug.com/1264839): it is confusing to rely on the observer
  // to notify `AppListSyncableService` of order change. Create an interface
  // to access sorting methods from here.
  for (AppListModelUpdaterObserver& observer : observers_) {
    observer.OnAppListPreferredOrderChanged(
        temporary_sort_manager_->temporary_order());
  }
}

void ChromeAppListModelUpdater::ClearOrder() {
  // TODO(https://crbug.com/1264839): it is confusing to rely on the observer
  // to notify `AppListSyncableService` of order change. Create an interface
  // to access sorting methods from here.
  for (AppListModelUpdaterObserver& observer : observers_)
    observer.OnAppListPreferredOrderChanged(ash::AppListSortOrder::kCustom);
}
