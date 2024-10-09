// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_settings_service.h"

#include <stddef.h>

#include <set>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/managed_user_setting_specifics.pb.h"

namespace supervised_user {

using base::JSONReader;
using base::UserMetricsAction;
using base::Value;
using syncer::DataType;
using syncer::ModelError;
using syncer::SUPERVISED_USER_SETTINGS;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;

const char kAtomicSettings[] = "atomic_settings";
const char kSupervisedUserInternalItemPrefix[] = "X-";
const char kQueuedItems[] = "queued_items";
const char kSplitSettingKeySeparator = ':';
const char kSplitSettings[] = "split_settings";

namespace {

bool SettingShouldApplyToPrefs(const std::string& name) {
  return !base::StartsWith(name, kSupervisedUserInternalItemPrefix,
                           base::CompareCase::INSENSITIVE_ASCII);
}

bool SyncChangeIsNewWebsiteApproval(const std::string& name,
                                    SyncChange::SyncChangeType change_type,
                                    base::Value* old_value,
                                    base::Value* new_value) {
  bool is_host_permission_change =
      base::StartsWith(name, supervised_user::kContentPackManualBehaviorHosts,
                       base::CompareCase::INSENSITIVE_ASCII);
  if (!is_host_permission_change) {
    return false;
  }
  switch (change_type) {
    case SyncChange::ACTION_ADD:
    case SyncChange::ACTION_UPDATE: {
      DCHECK(new_value && new_value->is_bool());
      // The change is a new approval if the new value is true, i.e. a new host
      // is manually allowlisted.
      return new_value->GetIfBool().value_or(false);
    }
    case SyncChange::ACTION_DELETE: {
      DCHECK(old_value && old_value->is_bool());
      // The change is a new approval if the old value was false, i.e. a host
      // that was manually blocked isn't anymore.
      return !old_value->GetIfBool().value_or(true);
    }
    default: {
      NOTREACHED_IN_MIGRATION();
      return false;
    }
  }
}

}  // namespace

SupervisedUserSettingsService::SupervisedUserSettingsService()
    : active_(false), initialization_failed_(false) {}

SupervisedUserSettingsService::~SupervisedUserSettingsService() = default;

void SupervisedUserSettingsService::Init(
    base::FilePath profile_path,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    bool load_synchronously) {
  base::FilePath path =
      profile_path.Append(supervised_user::kSupervisedUserSettingsFilename);
  PersistentPrefStore* store = new JsonPrefStore(
      path, std::unique_ptr<PrefFilter>(), std::move(sequenced_task_runner));
  Init(store);
  if (load_synchronously) {
    store_->ReadPrefs();
    DCHECK(IsReady());
  } else {
    store_->ReadPrefsAsync(nullptr);
  }
}

void SupervisedUserSettingsService::Init(
    scoped_refptr<PersistentPrefStore> store) {
  DCHECK(!store_.get());
  store_ = store;
  store_->AddObserver(this);
}

base::CallbackListSubscription
SupervisedUserSettingsService::SubscribeForSettingsChange(
    const SettingsCallback& callback) {
  if (IsReady()) {
    base::Value::Dict settings = GetSettingsWithDefault();
    callback.Run(std::move(settings));
  }

  return settings_callback_list_.Add(callback);
}

base::CallbackListSubscription
SupervisedUserSettingsService::SubscribeForNewWebsiteApproval(
    const WebsiteApprovalCallback& callback) {
  return website_approval_callback_list_.Add(callback);
}

void SupervisedUserSettingsService::RecordLocalWebsiteApproval(
    const std::string& host) {
  // Write the sync setting.
  std::string setting_key = MakeSplitSettingKey(
      supervised_user::kContentPackManualBehaviorHosts, host);
  SaveItem(setting_key, base::Value(true));

  // Now notify subscribers of the updates.
  website_approval_callback_list_.Notify(setting_key);
}

base::CallbackListSubscription
SupervisedUserSettingsService::SubscribeForShutdown(
    const ShutdownCallback& callback) {
  return shutdown_callback_list_.Add(callback);
}

bool SupervisedUserSettingsService::IsCustomPassphraseAllowed() const {
  return !active_;
}

void SupervisedUserSettingsService::SetActive(bool active) {
  active_ = active;

  if (active_) {
    // Child account supervised users must be signed in.
    SetLocalSetting(supervised_user::kSigninAllowed, base::Value(true));
    SetLocalSetting(supervised_user::kSigninAllowedOnNextStartup,
                    base::Value(true));

    // Always allow cookies, to avoid website compatibility issues.
    SetLocalSetting(supervised_user::kCookiesAlwaysAllowed, base::Value(true));

    // SafeSearch and GeolocationDisabled are controlled at the account level,
    // so don't override them client-side.
  } else {
    RemoveLocalSetting(supervised_user::kSigninAllowed);
    RemoveLocalSetting(supervised_user::kCookiesAlwaysAllowed);
    RemoveLocalSetting(supervised_user::kGeolocationDisabled);
  }

  InformSubscribers();
}

bool SupervisedUserSettingsService::IsReady() const {
  // Initialization cannot be complete but have failed at the same time.
  DCHECK(!(store_->IsInitializationComplete() && initialization_failed_));
  return initialization_failed_ || store_->IsInitializationComplete();
}

void SupervisedUserSettingsService::Clear() {
  store_->RemoveValue(kAtomicSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->RemoveValue(kSplitSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

// static
std::string SupervisedUserSettingsService::MakeSplitSettingKey(
    const std::string& prefix,
    const std::string& key) {
  return prefix + kSplitSettingKeySeparator + key;
}

void SupervisedUserSettingsService::SaveItem(
    const std::string& key,
    base::Value value) {
  // Update the value in our local dict, and push the changes to sync.
  std::string key_suffix = key;
  base::Value::Dict* dict = nullptr;
  if (sync_processor_) {
    base::RecordAction(UserMetricsAction("ManagedUsers_UploadItem_Syncing"));
    dict = GetDictionaryAndSplitKey(&key_suffix);
    DCHECK(GetQueuedItems()->empty());
    SyncChangeList change_list;
    SyncData data = CreateSyncDataForSetting(key, value);
    SyncChange::SyncChangeType change_type = dict->Find(key_suffix)
                                                 ? SyncChange::ACTION_UPDATE
                                                 : SyncChange::ACTION_ADD;
    change_list.push_back(SyncChange(FROM_HERE, change_type, data));
    std::optional<ModelError> error =
        sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
    DCHECK(!error.has_value()) << error.value().ToString();
  } else {
    // Queue the item up to be uploaded when we start syncing
    // (in MergeDataAndStartSyncing()).
    base::RecordAction(UserMetricsAction("ManagedUsers_UploadItem_Queued"));
    dict = GetQueuedItems();
  }
  dict->Set(key_suffix,std::move(value));

  // Now notify subscribers of the updates.
  // For simplicity and consistency with ProcessSyncChanges() we notify both
  // settings keys.
  store_->ReportValueChanged(kAtomicSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->ReportValueChanged(kSplitSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  InformSubscribers();
}

void SupervisedUserSettingsService::SetLocalSetting(std::string_view key,
                                                    base::Value value) {
  local_settings_.Set(key, std::move(value));
  InformSubscribers();
}

void SupervisedUserSettingsService::SetLocalSetting(std::string_view key,
                                                    base::Value::Dict dict) {
  local_settings_.Set(key, std::move(dict));
  InformSubscribers();
}

void SupervisedUserSettingsService::RemoveLocalSetting(std::string_view key) {
  local_settings_.Remove(key);
  InformSubscribers();
}

// static
SyncData SupervisedUserSettingsService::CreateSyncDataForSetting(
    const std::string& name,
    const base::Value& value) {
  std::string json_value;
  base::JSONWriter::Write(value, &json_value);
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user_setting()->set_name(name);
  specifics.mutable_managed_user_setting()->set_value(json_value);
  return SyncData::CreateLocalData(name, name, specifics);
}

void SupervisedUserSettingsService::Shutdown() {
  // Allow calling `Shutdown()` even if `Init(...)` was never
  // invoked on the service.
  if (store_) {
    store_->RemoveObserver(this);
  }
  shutdown_callback_list_.Notify();
}

void SupervisedUserSettingsService::WaitUntilReadyToSync(
    base::OnceClosure done) {
  DCHECK(!wait_until_ready_to_sync_cb_);
  if (IsReady()) {
    std::move(done).Run();
  } else {
    // Wait until OnInitializationCompleted().
    wait_until_ready_to_sync_cb_ = std::move(done);
  }
}

std::optional<syncer::ModelError>
SupervisedUserSettingsService::MergeDataAndStartSyncing(
    DataType type,
    const SyncDataList& initial_sync_data,
    std::unique_ptr<SyncChangeProcessor> sync_processor) {
  DCHECK_EQ(SUPERVISED_USER_SETTINGS, type);
  sync_processor_ = std::move(sync_processor);

  std::set<std::string> seen_keys;
  for (const auto it : *GetAtomicSettings()) {
    seen_keys.insert(it.first);
  }
  // Getting number of split setting items.
  for (const auto it : *GetSplitSettings()) {
    const base::Value& split_setting = it.second;
    DCHECK(split_setting.is_dict());
    for (const auto jt : split_setting.GetDict()) {
      seen_keys.insert(MakeSplitSettingKey(it.first, jt.first));
    }
  }

  // Getting number of queued items.
  base::Value::Dict* queued_items = GetQueuedItems();

  // Clear all atomic and split settings, then recreate them from Sync data.
  Clear();
  std::set<std::string> added_sync_keys;
  for (const SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, sync_data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        sync_data.GetSpecifics().managed_user_setting();
    std::optional<base::Value> value =
        JSONReader::Read(supervised_user_setting.value());
    // Wrongly formatted input will cause null values.
    // SetKey below requires non-null values.
    if (!value) {
      DLOG(ERROR) << "Invalid managed user setting value: "
                  << supervised_user_setting.value()
                  << ". Values must be JSON values.";
      continue;
    }
    std::string name_suffix = supervised_user_setting.name();
    std::string name_key = name_suffix;
    base::Value::Dict* dict = GetDictionaryAndSplitKey(&name_suffix);
    dict->Set(name_suffix, std::move(*value));
    if (seen_keys.find(name_key) == seen_keys.end()) {
      added_sync_keys.insert(name_key);
    }
  }

  store_->ReportValueChanged(kAtomicSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->ReportValueChanged(kSplitSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  InformSubscribers();

  // Upload all the queued up items (either with an ADD or an UPDATE action,
  // depending on whether they already exist) and move them to split settings.
  SyncChangeList change_list;
  for (const auto it : *queued_items) {
    std::string key_suffix = it.first;
    std::string name_key = key_suffix;
    base::Value::Dict* dict = GetDictionaryAndSplitKey(&key_suffix);
    SyncData data = CreateSyncDataForSetting(it.first, it.second);
    SyncChange::SyncChangeType change_type = dict->Find(key_suffix)
                                                 ? SyncChange::ACTION_UPDATE
                                                 : SyncChange::ACTION_ADD;
    change_list.push_back(SyncChange(FROM_HERE, change_type, data));
    dict->Set(key_suffix, it.second.Clone());
  }
  queued_items->clear();

  // Process all the accumulated changes from the queued items.
  if (!change_list.empty()) {
    store_->ReportValueChanged(kQueuedItems,
                               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    return sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  }

  return std::nullopt;
}

void SupervisedUserSettingsService::StopSyncing(DataType type) {
  DCHECK_EQ(syncer::SUPERVISED_USER_SETTINGS, type);
  sync_processor_.reset();
}

SyncDataList SupervisedUserSettingsService::GetAllSyncDataForTesting(
    DataType type) const {
  DCHECK_EQ(syncer::SUPERVISED_USER_SETTINGS, type);
  SyncDataList data;
  for (const auto it : *GetAtomicSettings()) {
    data.push_back(CreateSyncDataForSetting(it.first, it.second));
  }
  for (const auto it : *GetSplitSettings()) {
    const base::Value& split_setting = it.second;
    DCHECK(split_setting.is_dict());
    for (const auto jt : split_setting.GetDict()) {
      data.push_back(CreateSyncDataForSetting(
          MakeSplitSettingKey(it.first, jt.first), jt.second));
    }
  }
  DCHECK_EQ(0u, GetQueuedItems()->size());
  return data;
}

std::optional<syncer::ModelError>
SupervisedUserSettingsService::ProcessSyncChanges(
    const base::Location& from_here,
    const SyncChangeList& change_list) {
  for (const SyncChange& sync_change : change_list) {
    SyncData data = sync_change.sync_data();
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        data.GetSpecifics().managed_user_setting();
    std::string key = supervised_user_setting.name();
    base::Value::Dict* dict = GetDictionaryAndSplitKey(&key);
    base::Value* old_value = dict->Find(key);
    base::Value old_value_for_delete;
    SyncChange::SyncChangeType change_type = sync_change.change_type();
    base::Value* new_value = nullptr;

    switch (change_type) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        std::optional<base::Value> value =
            JSONReader::Read(supervised_user_setting.value());
        if (old_value) {
          DLOG_IF(WARNING, change_type == SyncChange::ACTION_ADD)
              << "Value for key " << key << " already exists";
        } else {
          DLOG_IF(WARNING, change_type == SyncChange::ACTION_UPDATE)
              << "Value for key " << key << " doesn't exist yet";
        }
        DLOG_IF(WARNING, !value.has_value())
            << "Invalid supervised_user_setting: "
            << supervised_user_setting.value();
        if (!value.has_value()) {
          continue;
        }
        new_value = dict->Set(key, std::move(*value));
        break;
      }
      case SyncChange::ACTION_DELETE: {
        DLOG_IF(WARNING, !old_value)
            << "Trying to delete nonexistent key " << key;
        if (!old_value) {
          continue;
        }
        old_value_for_delete = old_value->Clone();
        old_value = &old_value_for_delete;
        dict->Remove(key);
        break;
      }
    }

    if (SyncChangeIsNewWebsiteApproval(supervised_user_setting.name(),
                                       change_type, old_value, new_value)) {
      website_approval_callback_list_.Notify(key);
    }
  }
  store_->ReportValueChanged(kAtomicSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->ReportValueChanged(kSplitSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  InformSubscribers();

  return std::nullopt;
}

base::WeakPtr<syncer::SyncableService>
SupervisedUserSettingsService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SupervisedUserSettingsService::OnInitializationCompleted(bool success) {
  if (!success) {
    // If this happens, it means the profile directory was not found. There is
    // not much we can do, but the whole profile will probably be useless
    // anyway. Just mark initialization as failed and continue otherwise,
    // because subscribers might still expect to be called back.
    initialization_failed_ = true;
  }

  DCHECK(IsReady());

  if (wait_until_ready_to_sync_cb_) {
    std::move(wait_until_ready_to_sync_cb_).Run();
  }

  InformSubscribers();
}

const base::Value::Dict& SupervisedUserSettingsService::LocalSettingsForTest()
    const {
  return local_settings_;
}

base::Value::Dict* SupervisedUserSettingsService::GetDictionaryAndSplitKey(
    std::string* key) const {
  size_t pos = key->find_first_of(kSplitSettingKeySeparator);
  if (pos == std::string::npos) {
    return GetAtomicSettings();
  }

  base::Value::Dict* split_settings = GetSplitSettings();
  std::string prefix = key->substr(0, pos);
  base::Value::Dict* dict = split_settings->EnsureDict(prefix);
  key->erase(0, pos + 1);
  return dict;
}

base::Value::Dict* SupervisedUserSettingsService::GetOrCreateDictionary(
    const std::string& key) const {
  base::Value* value = nullptr;
  if (!store_->GetMutableValue(key, &value)) {
    store_->SetValue(key, base::Value(base::Value::Dict()),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    store_->GetMutableValue(key, &value);
  }
  DCHECK(value->is_dict());
  return &value->GetDict();
}

base::Value::Dict* SupervisedUserSettingsService::GetAtomicSettings() const {
  return GetOrCreateDictionary(kAtomicSettings);
}

base::Value::Dict* SupervisedUserSettingsService::GetSplitSettings() const {
  return GetOrCreateDictionary(kSplitSettings);
}

base::Value::Dict* SupervisedUserSettingsService::GetQueuedItems() const {
  return GetOrCreateDictionary(kQueuedItems);
}

base::Value::Dict SupervisedUserSettingsService::GetSettingsWithDefault() {
  DCHECK(IsReady());
  if (!active_ || initialization_failed_) {
    return base::Value::Dict();
  }

  base::Value::Dict settings(local_settings_.Clone());

  base::Value::Dict* atomic_settings = GetAtomicSettings();
  for (const auto it : *atomic_settings) {
    if (!SettingShouldApplyToPrefs(it.first)) {
      continue;
    }

    settings.Set(it.first, it.second.Clone());
  }

  base::Value::Dict* split_settings = GetSplitSettings();
  for (const auto it : *split_settings) {
    if (!SettingShouldApplyToPrefs(it.first)) {
      continue;
    }

    settings.Set(it.first, it.second.Clone());
  }

  return settings;
}

void SupervisedUserSettingsService::InformSubscribers() {
  if (!IsReady()) {
    return;
  }

  base::Value::Dict settings = GetSettingsWithDefault();
  settings_callback_list_.Notify(std::move(settings));
}

}  // namespace supervised_user
