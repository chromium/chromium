// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_model_builder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_model_builder.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/browser/ui/app_list/extension_app_model_builder.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_model_builder.h"
#include "chrome/browser/ui/app_list/page_break_app_item.h"
#include "chrome/browser/ui/app_list/page_break_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/protocol/sync.pb.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/one_shot_event.h"
#include "ui/base/l10n/l10n_util.h"

using syncer::SyncChange;

namespace app_list {

namespace {

constexpr char kNameKey[] = "name";
constexpr char kParentIdKey[] = "parent_id";
constexpr char kPositionKey[] = "position";
constexpr char kPinPositionKey[] = "pin_position";
constexpr char kTypeKey[] = "type";

void GetSyncSpecificsFromSyncItem(const AppListSyncableService::SyncItem* item,
                                  sync_pb::AppListSpecifics* specifics) {
  DCHECK(specifics);
  specifics->set_item_id(item->item_id);
  specifics->set_item_type(item->item_type);
  specifics->set_item_name(item->item_name);
  specifics->set_parent_id(item->parent_id);
  specifics->set_item_ordinal(item->item_ordinal.IsValid()
                                  ? item->item_ordinal.ToInternalValue()
                                  : std::string());
  specifics->set_item_pin_ordinal(item->item_pin_ordinal.IsValid()
                                      ? item->item_pin_ordinal.ToInternalValue()
                                      : std::string());
}

syncer::SyncData GetSyncDataFromSyncItem(
    const AppListSyncableService::SyncItem* item) {
  sync_pb::EntitySpecifics specifics;
  GetSyncSpecificsFromSyncItem(item, specifics.mutable_app_list());
  return syncer::SyncData::CreateLocalData(item->item_id, item->item_id,
                                           specifics);
}

bool AppIsDefault(extensions::ExtensionService* service,
                  const std::string& id) {
  return service && extensions::ExtensionPrefs::Get(service->profile())
                        ->WasInstalledByDefault(id);
}

bool IsUnRemovableDefaultApp(const std::string& id) {
  return id == extension_misc::kChromeAppId ||
         id == extensions::kWebStoreAppId ||
         id == file_manager::kFileManagerAppId ||
         id == extension_misc::kGeniusAppId;
}

void UninstallExtension(extensions::ExtensionService* service,
                        const std::string& id) {
  if (service && service->GetInstalledExtension(id)) {
    service->UninstallExtension(id, extensions::UNINSTALL_REASON_SYNC,
                                nullptr /* error */);
  }
}

sync_pb::AppListSpecifics::AppListItemType GetAppListItemType(
    const ChromeAppListItem* item) {
  if (item->is_folder())
    return sync_pb::AppListSpecifics::TYPE_FOLDER;
  else if (item->is_page_break())
    return sync_pb::AppListSpecifics::TYPE_PAGE_BREAK;
  else
    return sync_pb::AppListSpecifics::TYPE_APP;
}

void RemoveSyncItemFromLocalStorage(Profile* profile,
                                    const std::string& item_id) {
  DictionaryPrefUpdate(profile->GetPrefs(), prefs::kAppListLocalState)
      ->Remove(item_id, nullptr);
}

void UpdateSyncItemInLocalStorage(
    Profile* profile,
    const AppListSyncableService::SyncItem* sync_item) {
  DictionaryPrefUpdate pref_update(profile->GetPrefs(),
                                   prefs::kAppListLocalState);
  base::Value* dict_item = pref_update->FindKeyOfType(
      sync_item->item_id, base::Value::Type::DICTIONARY);
  if (!dict_item) {
    dict_item = pref_update->SetKey(sync_item->item_id,
                                    base::Value(base::Value::Type::DICTIONARY));
  }

  dict_item->SetKey(kNameKey, base::Value(sync_item->item_name));
  dict_item->SetKey(kParentIdKey, base::Value(sync_item->parent_id));
  dict_item->SetKey(kPositionKey,
                    base::Value(sync_item->item_ordinal.IsValid()
                                    ? sync_item->item_ordinal.ToInternalValue()
                                    : std::string()));
  dict_item->SetKey(
      kPinPositionKey,
      base::Value(sync_item->item_pin_ordinal.IsValid()
                      ? sync_item->item_pin_ordinal.ToInternalValue()
                      : std::string()));
  dict_item->SetKey(kTypeKey,
                    base::Value(static_cast<int>(sync_item->item_type)));
}

AppListSyncableService::ModelUpdaterFactoryCallback*
    g_model_updater_factory_callback_for_test_ = nullptr;

// Returns true if the sync item does not have parent.
bool IsTopLevelAppItem(const AppListSyncableService::SyncItem& sync_item) {
  return sync_item.parent_id.empty();
}

// Returns true if the sync item is a page break item.
bool IsPageBreakItem(const AppListSyncableService::SyncItem& sync_item) {
  return sync_item.item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK;
}

}  // namespace

// AppListSyncableService::ScopedModelUpdaterFactoryForTest

AppListSyncableService::ScopedModelUpdaterFactoryForTest::
    ScopedModelUpdaterFactoryForTest(
        const ModelUpdaterFactoryCallback& factory) {
  DCHECK(factory);
  factory_ = factory;
  g_model_updater_factory_callback_for_test_ = &factory_;
}

AppListSyncableService::ScopedModelUpdaterFactoryForTest::
    ~ScopedModelUpdaterFactoryForTest() {
  DCHECK_EQ(&factory_, g_model_updater_factory_callback_for_test_);
  g_model_updater_factory_callback_for_test_ = nullptr;
}

// AppListSyncableService::SyncItem

AppListSyncableService::SyncItem::SyncItem(
    const std::string& id,
    sync_pb::AppListSpecifics::AppListItemType type)
    : item_id(id), item_type(type) {}

AppListSyncableService::SyncItem::~SyncItem() = default;

// AppListSyncableService::ModelUpdaterDelegate

class AppListSyncableService::ModelUpdaterDelegate
    : public AppListModelUpdaterDelegate {
 public:
  explicit ModelUpdaterDelegate(AppListSyncableService* owner) : owner_(owner) {
    DVLOG(2) << owner_ << ": ModelUpdaterDelegate Added";
    owner_->GetModelUpdater()->SetDelegate(this);
  }

  ~ModelUpdaterDelegate() override {
    owner_->GetModelUpdater()->SetDelegate(nullptr);
    DVLOG(2) << owner_ << ": ModelUpdaterDelegate Removed";
  }

 private:
  // ChromeAppListModelUpdaterDelegate
  void OnAppListItemAdded(ChromeAppListItem* item) override {
    DCHECK(adding_item_id_.empty());
    adding_item_id_ = item->id();  // Ignore updates while adding an item.
    VLOG(2) << owner_ << " OnAppListItemAdded: " << item->ToDebugString();
    owner_->AddOrUpdateFromSyncItem(item);
    adding_item_id_.clear();

    // Sync OEM name if it was created on demand on ash side.
    if (item->id() == ash::kOemFolderId &&
        item->name() != owner_->oem_folder_name_) {
      item->SetName(owner_->oem_folder_name_);
    }
  }

  void OnAppListItemWillBeDeleted(ChromeAppListItem* item) override {
    DCHECK(adding_item_id_.empty());
    VLOG(2) << owner_ << " OnAppListItemDeleted: " << item->ToDebugString();
    // Don't sync folder removal in case the folder still exists on another
    // device (e.g. with device specific items in it). Empty folders will be
    // deleted when the last item is removed (in PruneEmptySyncFolders()).
    if (item->is_folder())
      return;

    if (item->GetItemType() == ArcAppItem::kItemType) {
      // Don't sync remove changes coming as result of disabling ARC.
      if (!arc::IsArcPlayStoreEnabledForProfile(owner_->profile()))
        return;
    }

    owner_->RemoveSyncItem(item->id());
  }

  void OnAppListItemUpdated(ChromeAppListItem* item) override {
    if (!adding_item_id_.empty()) {
      // Adding an item may trigger update notifications which should be
      // ignored.
      DCHECK_EQ(adding_item_id_, item->id());
      return;
    }
    VLOG(2) << owner_ << " OnAppListItemUpdated: " << item->ToDebugString();
    owner_->UpdateSyncItem(item);
  }

  AppListSyncableService* owner_;
  std::string adding_item_id_;

  DISALLOW_COPY_AND_ASSIGN(ModelUpdaterDelegate);
};

// AppListSyncableService

// static
void AppListSyncableService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kAppListLocalState);
}

AppListSyncableService::AppListSyncableService(
    Profile* profile,
    extensions::ExtensionSystem* extension_system)
    : profile_(profile),
      extension_system_(extension_system),
      initial_sync_data_processed_(false),
      first_app_list_sync_(true),
      weak_ptr_factory_(this) {
  if (g_model_updater_factory_callback_for_test_)
    model_updater_ = g_model_updater_factory_callback_for_test_->Run();
  else
    model_updater_ = std::make_unique<ChromeAppListModelUpdater>(profile);

  if (!extension_system) {
    LOG(ERROR) << "AppListSyncableService created with no ExtensionSystem";
    return;
  }

  oem_folder_name_ =
      l10n_util::GetStringUTF8(IDS_APP_LIST_OEM_DEFAULT_FOLDER_NAME);

  if (IsExtensionServiceReady()) {
    BuildModel();
  } else {
    extension_system_->ready().Post(
        FROM_HERE, base::Bind(&AppListSyncableService::BuildModel,
                              weak_ptr_factory_.GetWeakPtr()));
  }
}

AppListSyncableService::~AppListSyncableService() {
  // Remove observers.
  model_updater_delegate_.reset();
}

bool AppListSyncableService::IsExtensionServiceReady() const {
  return extension_system_->extension_service() &&
         extension_system_->extension_service()->is_ready();
}

void AppListSyncableService::InitFromLocalStorage() {
  // This should happen before sync and model is built.
  DCHECK(!sync_processor_.get());
  DCHECK(!IsInitialized());

  // Restore initial state from local storage.
  const base::DictionaryValue* local_items =
      profile_->GetPrefs()->GetDictionary(prefs::kAppListLocalState);
  DCHECK(local_items);

  for (base::DictionaryValue::Iterator item(*local_items); !item.IsAtEnd();
       item.Advance()) {
    const base::DictionaryValue* dict_item;
    if (!item.value().GetAsDictionary(&dict_item)) {
      LOG(ERROR) << "Dictionary not found for " << item.key() + ".";
      continue;
    }

    int type;
    if (!dict_item->GetInteger(kTypeKey, &type)) {
      LOG(ERROR) << "Item type is not set in local storage for " << item.key()
                 << ".";
      continue;
    }

    SyncItem* sync_item = CreateSyncItem(
        item.key(),
        static_cast<sync_pb::AppListSpecifics::AppListItemType>(type));

    dict_item->GetString(kNameKey, &sync_item->item_name);
    dict_item->GetString(kParentIdKey, &sync_item->parent_id);
    std::string position;
    std::string pin_position;
    dict_item->GetString(kPositionKey, &position);
    dict_item->GetString(kPinPositionKey, &pin_position);
    if (!position.empty())
      sync_item->item_ordinal = syncer::StringOrdinal(position);
    if (!pin_position.empty())
      sync_item->item_pin_ordinal = syncer::StringOrdinal(pin_position);
    ProcessNewSyncItem(sync_item);
  }
}

bool AppListSyncableService::IsInitialized() const {
  return apps_builder_.get();
}

void AppListSyncableService::BuildModel() {
  InitFromLocalStorage();

  DCHECK(IsExtensionServiceReady());
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListControllerDelegate* controller = client;
  apps_builder_ = std::make_unique<ExtensionAppModelBuilder>(controller);
  if (arc::IsArcAllowedForProfile(profile_))
    arc_apps_builder_ = std::make_unique<ArcAppModelBuilder>(controller);
  if (crostini::IsCrostiniUIAllowedForProfile(profile_)) {
    crostini_apps_builder_ =
        std::make_unique<CrostiniAppModelBuilder>(controller);
  }
  internal_apps_builder_ =
      std::make_unique<InternalAppModelBuilder>(controller);

  DCHECK(profile_);
  SyncStarted();
  apps_builder_->Initialize(this, profile_, model_updater_.get());
  if (arc_apps_builder_.get())
    arc_apps_builder_->Initialize(this, profile_, model_updater_.get());
  if (crostini_apps_builder_.get())
    crostini_apps_builder_->Initialize(this, profile_, model_updater_.get());
  internal_apps_builder_->Initialize(this, profile_, model_updater_.get());

  HandleUpdateFinished();
}

void AppListSyncableService::AddObserverAndStart(Observer* observer) {
  observer_list_.AddObserver(observer);
  SyncStarted();
}

void AppListSyncableService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppListSyncableService::NotifyObserversSyncUpdated() {
  for (auto& observer : observer_list_)
    observer.OnSyncModelUpdated();
}

size_t AppListSyncableService::GetNumSyncItemsForTest() {
  DCHECK(IsInitialized());
  return sync_items_.size();
}

const AppListSyncableService::SyncItem* AppListSyncableService::GetSyncItem(
    const std::string& id) const {
  auto iter = sync_items_.find(id);
  if (iter != sync_items_.end())
    return iter->second.get();
  return NULL;
}

void AppListSyncableService::SetOemFolderName(const std::string& name) {
  oem_folder_name_ = name;
  // Update OEM folder item if it was already created. If it is not created yet
  // then on creation it will take right name.
  ChromeAppListItem* oem_folder_item =
      model_updater_->FindItem(ash::kOemFolderId);
  if (oem_folder_item)
    oem_folder_item->SetName(oem_folder_name_);
}

AppListModelUpdater* AppListSyncableService::GetModelUpdater() {
  return model_updater_.get();
}

void AppListSyncableService::HandleUpdateStarted() {
  // Don't observe the model while processing update changes.
  model_updater_delegate_.reset();
}

void AppListSyncableService::HandleUpdateFinished() {
  // Processing an update may create folders without setting their positions.
  // Resolve them now.
  ResolveFolderPositions();

  // Resume or start observing app list model changes.
  model_updater_delegate_ = std::make_unique<ModelUpdaterDelegate>(this);

  NotifyObserversSyncUpdated();
}

void AppListSyncableService::AddItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  SyncItem* sync_item = FindOrAddSyncItem(app_item.get());
  if (!sync_item)
    return;  // Item is not valid.

  if (AppIsOem(app_item->id())) {
    VLOG(2) << this << ": AddItem to OEM folder: " << sync_item->ToString();
    model_updater_->AddItemToOemFolder(
        std::move(app_item), FindSyncItem(ash::kOemFolderId), oem_folder_name_,
        GetPreferredOemFolderPos());
  } else {
    std::string folder_id = sync_item->parent_id;
    VLOG(2) << this << ": AddItem: " << sync_item->ToString() << " Folder: '"
            << folder_id << "'";
    model_updater_->AddItemToFolder(std::move(app_item), folder_id);
  }

  PruneRedundantPageBreakItems();
}

AppListSyncableService::SyncItem* AppListSyncableService::FindOrAddSyncItem(
    const ChromeAppListItem* app_item) {
  const std::string& item_id = app_item->id();
  if (item_id.empty()) {
    LOG(ERROR) << "ChromeAppListItem item with empty ID";
    return NULL;
  }
  SyncItem* sync_item = FindSyncItem(item_id);
  if (sync_item) {
    // If there is an existing, non-REMOVE_DEFAULT entry, return it.
    if (sync_item->item_type !=
        sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
      DVLOG(2) << this << ": AddItem already exists: " << sync_item->ToString();
      return sync_item;
    }

    if (RemoveDefaultApp(app_item, sync_item))
      return NULL;

    // Fall through. The REMOVE_DEFAULT_APP entry has been deleted, now a new
    // App entry can be added.
  }

  return CreateSyncItemFromAppItem(app_item);
}

AppListSyncableService::SyncItem*
AppListSyncableService::CreateSyncItemFromAppItem(
    const ChromeAppListItem* app_item) {
  sync_pb::AppListSpecifics::AppListItemType type =
      GetAppListItemType(app_item);
  VLOG(2) << this << " CreateSyncItemFromAppItem:" << app_item->ToDebugString();
  SyncItem* sync_item = CreateSyncItem(app_item->id(), type);
  DCHECK(app_item->position().IsValid());
  UpdateSyncItemFromAppItem(app_item, sync_item);
  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, SyncChange::ACTION_ADD);
  return sync_item;
}

syncer::StringOrdinal AppListSyncableService::GetPinPosition(
    const std::string& app_id) {
  SyncItem* sync_item = FindSyncItem(app_id);
  if (!sync_item)
    return syncer::StringOrdinal();
  return sync_item->item_pin_ordinal;
}

void AppListSyncableService::SetPinPosition(
    const std::string& app_id,
    const syncer::StringOrdinal& item_pin_ordinal) {
  // Pin position can be set only after model is built.
  DCHECK(IsInitialized());
  SyncItem* sync_item = FindSyncItem(app_id);
  SyncChange::SyncChangeType sync_change_type;
  if (sync_item) {
    sync_change_type = SyncChange::ACTION_UPDATE;
  } else {
    sync_item = CreateSyncItem(app_id, sync_pb::AppListSpecifics::TYPE_APP);
    sync_change_type = SyncChange::ACTION_ADD;
  }

  sync_item->item_pin_ordinal = item_pin_ordinal;
  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, sync_change_type);
}

void AppListSyncableService::AddOrUpdateFromSyncItem(
    const ChromeAppListItem* app_item) {
  // Do not create a sync item for the OEM folder here, do that in
  // ResolveFolderPositions once the position has been resolved.
  if (app_item->id() == ash::kOemFolderId)
    return;

  DCHECK(app_item->position().IsValid());

  SyncItem* sync_item = FindSyncItem(app_item->id());
  if (sync_item) {
    model_updater_->UpdateAppItemFromSyncItem(
        sync_item,
        sync_item->item_id !=
            ash::kOemFolderId,  // Don't sync oem folder's name.
        false);                 // Don't sync its folder here.
    if (!sync_item->item_ordinal.IsValid()) {
      UpdateSyncItem(app_item);
      VLOG(2) << "Flushing position to sync item " << sync_item;
    }
    return;
  }
  CreateSyncItemFromAppItem(app_item);
}

bool AppListSyncableService::RemoveDefaultApp(const ChromeAppListItem* item,
                                              SyncItem* sync_item) {
  CHECK_EQ(sync_item->item_type,
           sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP);

  // If there is an existing REMOVE_DEFAULT_APP entry, and the app is
  // installed as a Default app, uninstall the app instead of adding it.
  if (sync_item->item_type == sync_pb::AppListSpecifics::TYPE_APP &&
      AppIsDefault(extension_system_->extension_service(), item->id())) {
    VLOG(2) << this
            << ": HandleDefaultApp: Uninstall: " << sync_item->ToString();
    UninstallExtension(extension_system_->extension_service(), item->id());
    return true;
  }

  // Otherwise, we are adding the app as a non-default app (i.e. an app that
  // was installed by Default and removed is getting installed explicitly by
  // the user), so delete the REMOVE_DEFAULT_APP.
  DeleteSyncItem(sync_item->item_id);
  return false;
}

void AppListSyncableService::DeleteSyncItem(const std::string& item_id) {
  SyncItem* sync_item = FindSyncItem(item_id);
  if (!sync_item) {
    LOG(ERROR) << "DeleteSyncItem: no sync item: " << item_id;
    return;
  }
  if (SyncStarted()) {
    VLOG(2) << this << " -> SYNC DELETE: " << sync_item->ToString();
    SyncChange sync_change(FROM_HERE, SyncChange::ACTION_DELETE,
                           GetSyncDataFromSyncItem(sync_item));
    sync_processor_->ProcessSyncChanges(FROM_HERE,
                                        syncer::SyncChangeList(1, sync_change));
  }
  RemoveSyncItemFromLocalStorage(profile_, item_id);
  sync_items_.erase(item_id);
}

void AppListSyncableService::UpdateSyncItem(const ChromeAppListItem* app_item) {
  SyncItem* sync_item = FindSyncItem(app_item->id());
  if (!sync_item) {
    LOG(ERROR) << "UpdateItem: no sync item: " << app_item->id();
    return;
  }
  bool changed = UpdateSyncItemFromAppItem(app_item, sync_item);
  if (!changed) {
    DVLOG(2) << this << " - Update: SYNC NO CHANGE: " << sync_item->ToString();
    return;
  }
  UpdateSyncItemInLocalStorage(profile_, sync_item);
  SendSyncChange(sync_item, SyncChange::ACTION_UPDATE);
  PruneRedundantPageBreakItems();
}

void AppListSyncableService::RemoveItem(const std::string& id) {
  RemoveSyncItem(id);
  model_updater_->RemoveItem(id);
  PruneEmptySyncFolders();
  PruneRedundantPageBreakItems();
}

void AppListSyncableService::RemoveUninstalledItem(const std::string& id) {
  RemoveSyncItem(id);
  model_updater_->RemoveUninstalledItem(id);
  PruneEmptySyncFolders();
  PruneRedundantPageBreakItems();
}

void AppListSyncableService::UpdateItem(const ChromeAppListItem* app_item) {
  // Check to see if the item needs to be moved to/from the OEM folder.
  bool is_oem = AppIsOem(app_item->id());
  if (!is_oem && app_item->folder_id() == ash::kOemFolderId)
    model_updater_->MoveItemToFolder(app_item->id(), "");
  else if (is_oem && app_item->folder_id() != ash::kOemFolderId)
    model_updater_->MoveItemToFolder(app_item->id(), ash::kOemFolderId);
}

void AppListSyncableService::RemoveSyncItem(const std::string& id) {
  VLOG(2) << this << ": RemoveSyncItem: " << id.substr(0, 8);
  auto iter = sync_items_.find(id);
  if (iter == sync_items_.end()) {
    DVLOG(2) << this << " : RemoveSyncItem: No Item.";
    return;
  }

  // Check for existing RemoveDefault sync item.
  SyncItem* sync_item = iter->second.get();
  sync_pb::AppListSpecifics::AppListItemType type = sync_item->item_type;
  if (type == sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    // RemoveDefault item exists, just return.
    DVLOG(2) << this << " : RemoveDefault Item exists.";
    return;
  }

  if (type == sync_pb::AppListSpecifics::TYPE_APP &&
      AppIsDefault(extension_system_->extension_service(), id)) {
    // This is a Default app; update the entry to a REMOVE_DEFAULT entry. This
    // will overwrite any existing entry for the item.
    VLOG(2) << this
            << " -> SYNC UPDATE: REMOVE_DEFAULT: " << sync_item->item_id;
    sync_item->item_type = sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP;
    UpdateSyncItemInLocalStorage(profile_, sync_item);
    SendSyncChange(sync_item, SyncChange::ACTION_UPDATE);
    return;
  }

  DeleteSyncItem(iter->first);
}

void AppListSyncableService::ResolveFolderPositions() {
  VLOG(1) << "ResolveFolderPositions.";
  for (const auto& sync_pair : sync_items_) {
    SyncItem* sync_item = sync_pair.second.get();
    if (sync_item->item_type != sync_pb::AppListSpecifics::TYPE_FOLDER)
      continue;

    model_updater_->UpdateAppItemFromSyncItem(
        sync_item,
        sync_item->item_id !=
            ash::kOemFolderId,  // Don't sync oem folder's name.
        false);                 // Don't sync its folder here.
  }

  // Move the OEM folder if one exists and we have not synced its position.
  if (!FindSyncItem(ash::kOemFolderId)) {
    model_updater_->ResolveOemFolderPosition(
        GetPreferredOemFolderPos(),
        base::BindOnce(
            [](base::WeakPtr<AppListSyncableService> self,
               ChromeAppListItem* oem_folder) {
              if (!self)
                return;

              if (oem_folder) {
                VLOG(1) << "Creating new OEM folder sync item: "
                        << oem_folder->position().ToDebugString();
                self->CreateSyncItemFromAppItem(oem_folder);
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void AppListSyncableService::PruneEmptySyncFolders() {
  std::set<std::string> parent_ids;
  for (const auto& sync_pair : sync_items_)
    parent_ids.insert(sync_pair.second->parent_id);

  for (auto iter = sync_items_.begin(); iter != sync_items_.end();) {
    SyncItem* sync_item = (iter++)->second.get();
    if (sync_item->item_type != sync_pb::AppListSpecifics::TYPE_FOLDER)
      continue;
    if (!base::ContainsKey(parent_ids, sync_item->item_id))
      DeleteSyncItem(sync_item->item_id);
  }
}

// AppListSyncableService syncer::SyncableService

void AppListSyncableService::InstallDefaultPageBreaksForTest() {
  InstallDefaultPageBreaks();
}

syncer::SyncMergeResult AppListSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(error_handler.get());

  const bool first_time_user = initial_sync_data.empty();
  if (first_time_user) {
    // Post a task to avoid adding the default page break items which can cause
    // sync changes during sync startup.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppListSyncableService::InstallDefaultPageBreaks,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  HandleUpdateStarted();

  // Reset local state and recreate from sync info.
  DictionaryPrefUpdate pref_update(profile_->GetPrefs(),
                                   prefs::kAppListLocalState);
  pref_update->Clear();

  sync_processor_ = std::move(sync_processor);
  sync_error_handler_ = std::move(error_handler);

  syncer::SyncMergeResult result = syncer::SyncMergeResult(type);
  result.set_num_items_before_association(sync_items_.size());
  VLOG(1) << this << ": MergeDataAndStartSyncing: " << initial_sync_data.size();

  // Copy all sync items to |unsynced_items|.
  std::set<std::string> unsynced_items;
  for (const auto& sync_pair : sync_items_) {
    unsynced_items.insert(sync_pair.first);
  }

  // Create SyncItem entries for initial_sync_data.
  size_t new_items = 0, updated_items = 0;
  for (syncer::SyncDataList::const_iterator iter = initial_sync_data.begin();
       iter != initial_sync_data.end(); ++iter) {
    const syncer::SyncData& data = *iter;
    const std::string& item_id = data.GetSpecifics().app_list().item_id();
    const sync_pb::AppListSpecifics& specifics = data.GetSpecifics().app_list();
    DVLOG(2) << this << "  Initial Sync Item: " << item_id
             << " Type: " << specifics.item_type();
    DCHECK_EQ(syncer::APP_LIST, data.GetDataType());
    if (ProcessSyncItemSpecifics(specifics))
      ++new_items;
    else
      ++updated_items;
    if (specifics.item_type() != sync_pb::AppListSpecifics::TYPE_FOLDER &&
        !IsUnRemovableDefaultApp(item_id) && !AppIsOem(item_id) &&
        !AppIsDefault(extension_system_->extension_service(), item_id)) {
      VLOG(2) << "Syncing non-default item: " << item_id;
      first_app_list_sync_ = false;
    }
    unsynced_items.erase(item_id);
  }
  result.set_num_items_after_association(sync_items_.size());
  result.set_num_items_added(new_items);
  result.set_num_items_deleted(0);
  result.set_num_items_modified(updated_items);

  // Initial sync data has been processed, it is safe now to add new sync items.
  initial_sync_data_processed_ = true;

  // Send unsynced items. Does not affect |result|.
  syncer::SyncChangeList change_list;
  for (std::set<std::string>::iterator iter = unsynced_items.begin();
       iter != unsynced_items.end(); ++iter) {
    SyncItem* sync_item = FindSyncItem(*iter);
    // Sync can cause an item to change folders, causing an unsynced folder
    // item to be removed.
    if (!sync_item)
      continue;
    VLOG(2) << this << " -> SYNC ADD: " << sync_item->ToString();
    UpdateSyncItemInLocalStorage(profile_, sync_item);
    change_list.push_back(SyncChange(FROM_HERE, SyncChange::ACTION_ADD,
                                     GetSyncDataFromSyncItem(sync_item)));
  }

  // Fix items that do not contain valid app list position, required for
  // builds prior to M53 (crbug.com/677647).
  for (const auto& sync_pair : sync_items_) {
    SyncItem* sync_item = sync_pair.second.get();
    if (sync_item->item_type != sync_pb::AppListSpecifics::TYPE_APP ||
        sync_item->item_ordinal.IsValid()) {
      continue;
    }
    const ChromeAppListItem* app_item =
        model_updater_->FindItem(sync_item->item_id);
    if (app_item) {
      if (UpdateSyncItemFromAppItem(app_item, sync_item)) {
        VLOG(1) << "Fixing sync item from existing app: " << sync_item;
      } else {
        sync_item->item_ordinal = syncer::StringOrdinal::CreateInitialOrdinal();
        VLOG(1) << "Failed to fix sync item from existing app. "
                << "Generating new position ordinal: " << sync_item;
      }
    } else {
      sync_item->item_ordinal = syncer::StringOrdinal::CreateInitialOrdinal();
      VLOG(1) << "Fixing sync item by generating new position ordinal: "
              << sync_item;
    }
    change_list.push_back(SyncChange(FROM_HERE, SyncChange::ACTION_UPDATE,
                                     GetSyncDataFromSyncItem(sync_item)));
  }

  sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);

  HandleUpdateFinished();

  return result;
}

void AppListSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type, syncer::APP_LIST);

  sync_processor_.reset();
  sync_error_handler_.reset();
}

syncer::SyncDataList AppListSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(syncer::APP_LIST, type);

  VLOG(1) << this << ": GetAllSyncData: " << sync_items_.size();
  syncer::SyncDataList list;
  for (auto iter = sync_items_.begin(); iter != sync_items_.end(); ++iter) {
    VLOG(2) << this << " -> SYNC: " << iter->second->ToString();
    list.push_back(GetSyncDataFromSyncItem(iter->second.get()));
  }
  return list;
}

syncer::SyncError AppListSyncableService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!sync_processor_.get()) {
    return syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                             "App List syncable service is not started.",
                             syncer::APP_LIST);
  }

  HandleUpdateStarted();

  VLOG(1) << this << ": ProcessSyncChanges: " << change_list.size();
  for (syncer::SyncChangeList::const_iterator iter = change_list.begin();
       iter != change_list.end(); ++iter) {
    const SyncChange& change = *iter;
    VLOG(2) << this << "  Change: "
            << change.sync_data().GetSpecifics().app_list().item_id() << " ("
            << change.change_type() << ")";
    if (change.change_type() == SyncChange::ACTION_ADD ||
        change.change_type() == SyncChange::ACTION_UPDATE) {
      ProcessSyncItemSpecifics(change.sync_data().GetSpecifics().app_list());
    } else if (change.change_type() == SyncChange::ACTION_DELETE) {
      DeleteSyncItemSpecifics(change.sync_data().GetSpecifics().app_list());
    } else {
      LOG(ERROR) << "Invalid sync change";
    }
  }

  HandleUpdateFinished();

  return syncer::SyncError();
}

void AppListSyncableService::Shutdown() {
  internal_apps_builder_.reset();
  crostini_apps_builder_.reset();
  arc_apps_builder_.reset();
  apps_builder_.reset();
}

// AppListSyncableService private

bool AppListSyncableService::ProcessSyncItemSpecifics(
    const sync_pb::AppListSpecifics& specifics) {
  const std::string& item_id = specifics.item_id();
  if (item_id.empty()) {
    LOG(ERROR) << "AppList item with empty ID";
    return false;
  }
  SyncItem* sync_item = FindSyncItem(item_id);
  if (sync_item) {
    // If an item of the same type exists, update it.
    if (sync_item->item_type == specifics.item_type()) {
      UpdateSyncItemFromSync(specifics, sync_item);
      ProcessExistingSyncItem(sync_item);
      UpdateSyncItemInLocalStorage(profile_, sync_item);
      VLOG(2) << this << " <- SYNC UPDATE: " << sync_item->ToString();
      return false;
    }
    // Otherwise, one of the entries should be TYPE_REMOVE_DEFAULT_APP.
    if (sync_item->item_type !=
            sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP &&
        specifics.item_type() !=
            sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
      LOG(ERROR) << "Synced item type: " << specifics.item_type()
                 << " != existing sync item type: " << sync_item->item_type
                 << " Deleting item from model!";
      model_updater_->RemoveItem(item_id);
    }
    VLOG(2) << this << " - ProcessSyncItem: Delete existing entry: "
            << sync_item->ToString();
    sync_items_.erase(item_id);
  }

  sync_item = CreateSyncItem(item_id, specifics.item_type());
  UpdateSyncItemFromSync(specifics, sync_item);
  ProcessNewSyncItem(sync_item);
  UpdateSyncItemInLocalStorage(profile_, sync_item);
  VLOG(2) << this << " <- SYNC ADD: " << sync_item->ToString();
  return true;
}

void AppListSyncableService::ProcessNewSyncItem(SyncItem* sync_item) {
  VLOG(2) << "ProcessNewSyncItem: " << sync_item->ToString();
  switch (sync_item->item_type) {
    case sync_pb::AppListSpecifics::TYPE_APP: {
      // New apps are added through ExtensionAppModelBuilder.
      // TODO(stevenjb): Determine how to handle app items in sync that
      // are not installed (e.g. default / OEM apps).
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP: {
      VLOG(1) << this << ": Uninstall: " << sync_item->ToString();
      UninstallExtension(extension_system_->extension_service(),
                         sync_item->item_id);
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_FOLDER: {
      // We don't create new folders here, the model will do that.
      model_updater_->UpdateAppItemFromSyncItem(
          sync_item,
          sync_item->item_id !=
              ash::kOemFolderId,  // Don't sync oem folder's name.
          false);                 // It's a folder itself.
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_URL: {
      // TODO(stevenjb): Implement
      LOG(WARNING) << "TYPE_URL not supported";
      return;
    }
    case sync_pb::AppListSpecifics::TYPE_PAGE_BREAK: {
      // This is can be either a default page break item that was installed by
      // default for new users, or a non-default page-break item that was added
      // by the user. the ctor of PageBreakAppItem will update the newly-created
      // item from its |sync_item|.
      model_updater_->AddItem(std::make_unique<PageBreakAppItem>(
          profile_, model_updater_.get(), sync_item, sync_item->item_id));
      return;
    }
  }
  NOTREACHED() << "Unrecognized sync item type: " << sync_item->ToString();
}

void AppListSyncableService::ProcessExistingSyncItem(SyncItem* sync_item) {
  if (sync_item->item_type ==
      sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    return;
  }
  VLOG(2) << "ProcessExistingSyncItem: " << sync_item->ToString();

  // The only place where sync can change an item's folder. Prevent moving OEM
  // item to the folder, other than OEM folder.
  const bool update_folder = !AppIsOem(sync_item->item_id);
  model_updater_->UpdateAppItemFromSyncItem(
      sync_item,
      sync_item->item_id != ash::kOemFolderId,  // Don't sync oem folder's name.
      update_folder);
}

bool AppListSyncableService::SyncStarted() {
  if (sync_processor_.get())
    return true;
  if (flare_.is_null()) {
    VLOG(1) << this << ": SyncStarted: Flare.";
    flare_ = sync_start_util::GetFlareForSyncableService(profile_->GetPath());
    flare_.Run(syncer::APP_LIST);
  }
  return false;
}

void AppListSyncableService::SendSyncChange(
    SyncItem* sync_item,
    SyncChange::SyncChangeType sync_change_type) {
  if (!SyncStarted()) {
    DVLOG(2) << this << " - SendSyncChange: SYNC NOT STARTED: "
             << sync_item->ToString();
    return;
  }
  if (!initial_sync_data_processed_ &&
      sync_change_type == SyncChange::ACTION_ADD) {
    // This can occur if an initial item is created before its folder item.
    // A sync item should already exist for the folder, so we do not want to
    // send an ADD event, since that would trigger a CHECK in the sync code.
    DCHECK(sync_item->item_type == sync_pb::AppListSpecifics::TYPE_FOLDER);
    DVLOG(2) << this << " - SendSyncChange: ADD before initial data processed: "
             << sync_item->ToString();
    return;
  }
  if (sync_change_type == SyncChange::ACTION_ADD)
    VLOG(2) << this << " -> SYNC ADD: " << sync_item->ToString();
  else
    VLOG(2) << this << " -> SYNC UPDATE: " << sync_item->ToString();
  SyncChange sync_change(FROM_HERE, sync_change_type,
                         GetSyncDataFromSyncItem(sync_item));
  sync_processor_->ProcessSyncChanges(FROM_HERE,
                                      syncer::SyncChangeList(1, sync_change));
}

AppListSyncableService::SyncItem* AppListSyncableService::FindSyncItem(
    const std::string& item_id) {
  auto iter = sync_items_.find(item_id);
  if (iter == sync_items_.end())
    return NULL;
  return iter->second.get();
}

AppListSyncableService::SyncItem* AppListSyncableService::CreateSyncItem(
    const std::string& item_id,
    sync_pb::AppListSpecifics::AppListItemType item_type) {
  DCHECK(!base::ContainsKey(sync_items_, item_id));
  sync_items_[item_id] = std::make_unique<SyncItem>(item_id, item_type);
  return sync_items_[item_id].get();
}

void AppListSyncableService::DeleteSyncItemSpecifics(
    const sync_pb::AppListSpecifics& specifics) {
  const std::string& item_id = specifics.item_id();
  if (item_id.empty()) {
    LOG(ERROR) << "Delete AppList item with empty ID";
    return;
  }
  VLOG(2) << this << ": DeleteSyncItemSpecifics: " << item_id.substr(0, 8);
  auto iter = sync_items_.find(item_id);
  if (iter == sync_items_.end())
    return;
  sync_pb::AppListSpecifics::AppListItemType item_type =
      iter->second->item_type;
  VLOG(2) << this << " <- SYNC DELETE: " << iter->second->ToString();
  RemoveSyncItemFromLocalStorage(profile_, item_id);
  sync_items_.erase(iter);

  // Only delete apps from the model. Folders will be deleted when all
  // children have been deleted.
  if (item_type == sync_pb::AppListSpecifics::TYPE_APP) {
    model_updater_->RemoveItem(item_id);
  }
}

syncer::StringOrdinal AppListSyncableService::GetPreferredOemFolderPos() {
  VLOG(1) << "GetPreferredOemFolderPos: " << first_app_list_sync_;
  if (!first_app_list_sync_) {
    VLOG(1) << "Sync items exist, placing OEM folder at end.";
    syncer::StringOrdinal last;
    for (const auto& sync_pair : sync_items_) {
      SyncItem* sync_item = sync_pair.second.get();
      if (sync_item->item_ordinal.IsValid() &&
          (!last.IsValid() || sync_item->item_ordinal.GreaterThan(last))) {
        last = sync_item->item_ordinal;
      }
    }
    if (last.IsValid())
      return last.CreateAfter();
  }
  return syncer::StringOrdinal();
}

bool AppListSyncableService::AppIsOem(const std::string& id) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  if (arc_prefs && arc_prefs->IsOem(id))
    return true;

  if (!extension_system_->extension_service())
    return false;
  const extensions::Extension* extension =
      extension_system_->extension_service()->GetExtensionById(id, true);
  return extension && extension->was_installed_by_oem();
}

std::string AppListSyncableService::SyncItem::ToString() const {
  std::string res = item_id.substr(0, 8);
  if (item_type == sync_pb::AppListSpecifics::TYPE_REMOVE_DEFAULT_APP) {
    res += " { RemoveDefault }";
  } else if (item_type == sync_pb::AppListSpecifics::TYPE_PAGE_BREAK) {
    res += " { PageBreakItem }";
    res += " [" + item_ordinal.ToDebugString() + "]";
  } else {
    res += " { " + item_name + " }";
    res += " [" + item_ordinal.ToDebugString() + "]";
    if (!parent_id.empty())
      res += " <" + parent_id.substr(0, 8) + ">";
    res += " [" + item_pin_ordinal.ToDebugString() + "]";
  }
  return res;
}

std::vector<AppListSyncableService::SyncItem*>
AppListSyncableService::GetSortedTopLevelSyncItems() const {
  // Filter out items in folder.
  std::vector<SyncItem*> sync_items;
  for (const auto& sync_pair : sync_items_) {
    const auto* sync_item = sync_pair.second.get();
    if (IsTopLevelAppItem(*sync_item) && sync_item->item_ordinal.IsValid())
      sync_items.emplace_back(sync_pair.second.get());
  }

  // Sort remaining items based on their positions.
  std::sort(sync_items.begin(), sync_items.end(),
            [](SyncItem* const& item1, SyncItem* const& item2) -> bool {
              return item1->item_ordinal.LessThan(item2->item_ordinal);
            });
  return sync_items;
}

void AppListSyncableService::PruneRedundantPageBreakItems() {
  auto top_level_sync_items = GetSortedTopLevelSyncItems();

  // If the first item is a "page break" item, delete it. If there are
  // contiguous "page break" items, delete duplicate.
  bool was_page_break = true;
  for (auto iter = top_level_sync_items.begin();
       iter != top_level_sync_items.end();) {
    if (!IsPageBreakItem(**iter)) {
      was_page_break = false;
      ++iter;
      continue;
    }
    auto current_iter = iter++;
    if (was_page_break) {
      DeleteSyncItem((*current_iter)->item_id);
      iter = top_level_sync_items.erase(current_iter);
    } else {
      was_page_break = true;
    }
  }

  // Remove the trailing "page break" item if it exists.
  if (!top_level_sync_items.empty() &&
      IsPageBreakItem(*top_level_sync_items.back())) {
    DeleteSyncItem(top_level_sync_items.back()->item_id);
  }

  // Remove all the "page break" items that are in folder. No such item should
  // exist in folder. It should be safe to remove them if it do occur.
  for (auto iter = sync_items_.begin(); iter != sync_items_.end();) {
    const auto* sync_item = (iter++)->second.get();
    if (IsTopLevelAppItem(*sync_item) || !IsPageBreakItem(*sync_item))
      continue;

    LOG(ERROR) << "Delete a page break item in folder: " << sync_item->item_id;
    DeleteSyncItem(sync_item->item_id);
  }
}

void AppListSyncableService::InstallDefaultPageBreaks() {
  for (size_t i = 0; i < app_list::kDefaultPageBreakAppIdsLength; ++i) {
    auto* const id = app_list::kDefaultPageBreakAppIds[i];
    auto* sync_item = GetSyncItem(id);
    if (sync_item) {
      // The user may have cleared their sync from
      // https://chrome.google.com/sync, so it may appear here that it's a new
      // user, while in fact on this device, it's not. We don't want to recreate
      // and re-add an already existing default page break item.
      continue;
    }

    auto page_break_item = std::make_unique<PageBreakAppItem>(
        profile(), model_updater_.get(), nullptr /* sync_item */, id);
    page_break_item->SetName("__default_page_break__");
    AddItem(std::move(page_break_item));
  }
}

void AppListSyncableService::UpdateSyncItemFromSync(
    const sync_pb::AppListSpecifics& specifics,
    AppListSyncableService::SyncItem* item) {
  DCHECK_EQ(item->item_id, specifics.item_id());
  item->item_type = specifics.item_type();
  item->item_name = specifics.item_name();

  // Ignore update to put item into the OEM folder in case app is not OEM. This
  // can happen when app is installed on several devices where app is OEM on one
  // device and not on another devices.
  if (specifics.parent_id() != ash::kOemFolderId || AppIsOem(item->item_id))
    item->parent_id = specifics.parent_id();
  if (specifics.has_item_ordinal())
    item->item_ordinal = syncer::StringOrdinal(specifics.item_ordinal());
  if (specifics.has_item_pin_ordinal()) {
    item->item_pin_ordinal =
        syncer::StringOrdinal(specifics.item_pin_ordinal());
  }
}

bool AppListSyncableService::UpdateSyncItemFromAppItem(
    const ChromeAppListItem* app_item,
    AppListSyncableService::SyncItem* sync_item) {
  DCHECK_EQ(sync_item->item_id, app_item->id());

  // Page breaker should not be added in a folder.
  DCHECK(!app_item->is_page_break() || app_item->folder_id().empty());

  bool changed = false;
  // Allow sync changes for parent only for non OEM app.
  if (sync_item->parent_id != app_item->folder_id() &&
      !AppIsOem(app_item->id())) {
    sync_item->parent_id = app_item->folder_id();
    changed = true;
  }
  if (sync_item->item_name != app_item->name()) {
    sync_item->item_name = app_item->name();
    changed = true;
  }
  if (!sync_item->item_ordinal.IsValid() ||
      !app_item->position().Equals(sync_item->item_ordinal)) {
    sync_item->item_ordinal = app_item->position();
    changed = true;
  }
  return changed;
}

}  // namespace app_list
