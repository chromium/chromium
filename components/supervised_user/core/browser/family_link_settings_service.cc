// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_link_settings_service.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
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
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/managed_user_setting_specifics.pb.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace supervised_user {

namespace {

using base::JSONReader;
using base::UserMetricsAction;
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

bool SettingShouldApplyToPrefs(const std::string& name) {
  return !base::StartsWith(name, kSupervisedUserInternalItemPrefix,
                           base::CompareCase::INSENSITIVE_ASCII);
}

bool SyncChangeIsNewWebsiteApproval(const std::string& name,
                                    SyncChange::SyncChangeType change_type,
                                    base::Value* old_value,
                                    base::Value* new_value) {
  bool is_host_permission_change =
      base::StartsWith(name, kContentPackManualBehaviorHosts,
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
      NOTREACHED();
    }
  }
}
}  // namespace

FamilyLinkSettingsService::FamilyLinkSettingsService()
    : active_(false), initialization_failed_(false) {}

FamilyLinkSettingsService::~FamilyLinkSettingsService() {
  if (wait_until_ready_to_sync_trap_) {
    SCOPED_CRASH_KEY_STRING32("SupervisedUser", "RaceInFLSSShutdown",
                              "InDestructor");
    base::debug::DumpWithoutCrashing();
  }
}

void FamilyLinkSettingsService::Init(
    base::FilePath profile_path,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    bool load_synchronously) {
  base::FilePath path = profile_path.Append(kSupervisedUserSettingsFilename);
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

void FamilyLinkSettingsService::Init(scoped_refptr<PersistentPrefStore> store) {
  DCHECK(!store_.get());
  store_ = store;
  store_->AddObserver(this);
}

base::CallbackListSubscription
FamilyLinkSettingsService::SubscribeForSettingsChange(
    const SettingsCallback& callback) {
  if (IsReady()) {
    base::DictValue settings = GetSettingsWithDefault();
    callback.Run(std::move(settings));
  }

  return settings_callback_list_.Add(callback);
}

base::CallbackListSubscription
FamilyLinkSettingsService::SubscribeForNewWebsiteApproval(
    const WebsiteApprovalCallback& callback) {
  return website_approval_callback_list_.Add(callback);
}

void FamilyLinkSettingsService::RecordLocalWebsiteApproval(
    const std::string& host) {
  // Write the sync setting.
  std::string setting_key =
      MakeSplitSettingKey(kContentPackManualBehaviorHosts, host);
  SaveItem(setting_key, base::Value(true));

  // Now notify subscribers of the updates.
  website_approval_callback_list_.Notify(setting_key);
}

base::CallbackListSubscription FamilyLinkSettingsService::SubscribeForShutdown(
    const ShutdownCallback& callback) {
  return shutdown_callback_list_.Add(callback);
}

bool FamilyLinkSettingsService::IsCustomPassphraseAllowed() const {
  return !active_;
}

void FamilyLinkSettingsService::SetActive(bool active) {
  active_ = active;

  if (active_) {
    // Child account supervised users must be signed in.
    SetLocalSetting(kSigninAllowed, base::Value(true));
    SetLocalSetting(kSigninAllowedOnNextStartup, base::Value(true));

    // Always allow cookies, to avoid website compatibility issues.
    SetLocalSetting(kCookiesAlwaysAllowed, base::Value(true));

    // SafeSearch and GeolocationDisabled are controlled at the account level,
    // so don't override them client-side.
  } else {
    RemoveLocalSetting(kSigninAllowed);
    RemoveLocalSetting(kCookiesAlwaysAllowed);
  }

  InformSubscribers();
}

bool FamilyLinkSettingsService::IsReady() const {
  // Initialization cannot be complete but have failed at the same time.
  DCHECK(!(store_->IsInitializationComplete() && initialization_failed_));
  return initialization_failed_ || store_->IsInitializationComplete();
}

void FamilyLinkSettingsService::Clear() {
  store_->RemoveValue(kAtomicSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->RemoveValue(kSplitSettings,
                      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

// static
std::string FamilyLinkSettingsService::MakeSplitSettingKey(
    const std::string& prefix,
    const std::string& key) {
  return prefix + kSplitSettingKeySeparator + key;
}

void FamilyLinkSettingsService::SaveItem(const std::string& key,
                                         base::Value value) {
  // Update the value in our local dict, and push the changes to sync.
  std::string key_suffix = key;
  base::DictValue* dict = nullptr;
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
  dict->Set(key_suffix, std::move(value));

  // Now notify subscribers of the updates.
  // For simplicity and consistency with ProcessSyncChanges() we notify both
  // settings keys.
  store_->ReportValueChanged(kAtomicSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  store_->ReportValueChanged(kSplitSettings,
                             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  InformSubscribers();
}

void FamilyLinkSettingsService::SetLocalSetting(std::string_view key,
                                                base::Value value) {
  local_settings_.Set(key, std::move(value));
  InformSubscribers();
}

void FamilyLinkSettingsService::SetLocalSetting(std::string_view key,
                                                base::DictValue dict) {
  local_settings_.Set(key, std::move(dict));
  InformSubscribers();
}

void FamilyLinkSettingsService::RemoveLocalSetting(std::string_view key) {
  local_settings_.Remove(key);
  InformSubscribers();
}

// static
SyncData FamilyLinkSettingsService::CreateSyncDataForSetting(
    const std::string& name,
    const base::Value& value) {
  std::string json_value = base::WriteJson(value).value_or("");
  ::sync_pb::EntitySpecifics specifics;
  specifics.mutable_managed_user_setting()->set_name(name);
  specifics.mutable_managed_user_setting()->set_value(json_value);
  return SyncData::CreateLocalData(name, name, specifics);
}

void FamilyLinkSettingsService::Shutdown() {
  if (wait_until_ready_to_sync_trap_) {
    SCOPED_CRASH_KEY_STRING32("SupervisedUser", "RaceInFLSSShutdown",
                              "InShutdown");
    base::debug::DumpWithoutCrashing();
    wait_until_ready_to_sync_trap_ = false;
  }

  // Allow calling `Shutdown()` even if `Init(...)` was never
  // invoked on the service.
  if (store_) {
    store_->RemoveObserver(this);
  }
  shutdown_callback_list_.Notify();
}

void FamilyLinkSettingsService::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK(!wait_until_ready_to_sync_cb_);
  if (IsReady()) {
    std::move(done).Run();
  } else {
    // Wait until OnInitializationCompleted().
    wait_until_ready_to_sync_cb_ = std::move(done);
  }
}

std::optional<syncer::ModelError>
FamilyLinkSettingsService::MergeDataAndStartSyncing(
    DataType type,
    const SyncDataList& initial_sync_data,
    std::unique_ptr<SyncChangeProcessor> sync_processor) {
  DCHECK_EQ(SUPERVISED_USER_SETTINGS, type);
  sync_processor_ = std::move(sync_processor);

  absl::flat_hash_set<std::string> seen_keys;
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
  base::DictValue* queued_items = GetQueuedItems();

  // Clear all atomic and split settings, then recreate them from Sync data.
  Clear();
  absl::flat_hash_set<std::string> added_sync_keys;
  for (const SyncData& sync_data : initial_sync_data) {
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, sync_data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        sync_data.GetSpecifics().managed_user_setting();
    std::optional<base::Value> value = JSONReader::Read(
        supervised_user_setting.value(), base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
    base::DictValue* dict = GetDictionaryAndSplitKey(&name_suffix);
    dict->Set(name_suffix, std::move(*value));
    if (!seen_keys.contains(name_key)) {
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
    base::DictValue* dict = GetDictionaryAndSplitKey(&key_suffix);
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

void FamilyLinkSettingsService::StopSyncing(DataType type) {
  DCHECK_EQ(syncer::SUPERVISED_USER_SETTINGS, type);
  sync_processor_.reset();
}

SyncDataList FamilyLinkSettingsService::GetAllSyncDataForTesting(
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

std::optional<syncer::ModelError> FamilyLinkSettingsService::ProcessSyncChanges(
    const base::Location& from_here,
    const SyncChangeList& change_list) {
  for (const SyncChange& sync_change : change_list) {
    SyncData data = sync_change.sync_data();
    DCHECK_EQ(SUPERVISED_USER_SETTINGS, data.GetDataType());
    const ::sync_pb::ManagedUserSettingSpecifics& supervised_user_setting =
        data.GetSpecifics().managed_user_setting();
    std::string key = supervised_user_setting.name();
    base::DictValue* dict = GetDictionaryAndSplitKey(&key);
    base::Value* old_value = dict->Find(key);
    base::Value old_value_for_delete;
    SyncChange::SyncChangeType change_type = sync_change.change_type();
    base::Value* new_value = nullptr;

    switch (change_type) {
      case SyncChange::ACTION_ADD:
      case SyncChange::ACTION_UPDATE: {
        std::optional<base::Value> value =
            JSONReader::Read(supervised_user_setting.value(),
                             base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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

base::WeakPtr<syncer::SyncableService> FamilyLinkSettingsService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<const FamilyLinkSettingsService>
FamilyLinkSettingsService::GetWeakPtr() const {
  return weak_ptr_factory_.GetWeakPtr();
}

std::string FamilyLinkSettingsService::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK(entity_data.specifics.has_managed_user_setting());
  return entity_data.specifics.managed_user_setting().name();
}

void FamilyLinkSettingsService::OnInitializationCompleted(bool success) {
  if (!success) {
    // If this happens, it means the profile directory was not found. There is
    // not much we can do, but the whole profile will probably be useless
    // anyway. Just mark initialization as failed and continue otherwise,
    // because subscribers might still expect to be called back.
    initialization_failed_ = true;
  }

  DCHECK(IsReady());

  if (wait_until_ready_to_sync_cb_) {
    wait_until_ready_to_sync_trap_ = true;
    std::move(wait_until_ready_to_sync_cb_).Run();
  }

  InformSubscribers();
  wait_until_ready_to_sync_trap_ = false;
}

const base::DictValue& FamilyLinkSettingsService::LocalSettingsForTest() const {
  return local_settings_;
}

base::DictValue* FamilyLinkSettingsService::GetDictionaryAndSplitKey(
    std::string* key) const {
  size_t pos = key->find_first_of(kSplitSettingKeySeparator);
  if (pos == std::string::npos) {
    return GetAtomicSettings();
  }

  base::DictValue* split_settings = GetSplitSettings();
  std::string prefix = key->substr(0, pos);
  base::DictValue* dict = split_settings->EnsureDict(prefix);
  key->erase(0, pos + 1);
  return dict;
}

base::DictValue* FamilyLinkSettingsService::GetOrCreateDictionary(
    std::string_view key) const {
  base::Value* value = nullptr;
  if (!store_->GetMutableValue(key, &value)) {
    store_->SetValue(key, base::Value(base::DictValue()),
                     WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    store_->GetMutableValue(key, &value);
  }
  DCHECK(value->is_dict());
  return &value->GetDict();
}

base::DictValue* FamilyLinkSettingsService::GetAtomicSettings() const {
  return GetOrCreateDictionary(kAtomicSettings);
}

base::DictValue* FamilyLinkSettingsService::GetSplitSettings() const {
  return GetOrCreateDictionary(kSplitSettings);
}

base::DictValue* FamilyLinkSettingsService::GetQueuedItems() const {
  return GetOrCreateDictionary(kQueuedItems);
}

base::DictValue FamilyLinkSettingsService::GetSettingsWithDefault() const {
  DCHECK(IsReady());
  if (!active_ || initialization_failed_) {
    return base::DictValue();
  }

  base::DictValue settings(local_settings_.Clone());

  base::DictValue* atomic_settings = GetAtomicSettings();
  for (const auto it : *atomic_settings) {
    if (!SettingShouldApplyToPrefs(it.first)) {
      continue;
    }

    settings.Set(it.first, it.second.Clone());
  }

  base::DictValue* split_settings = GetSplitSettings();
  for (const auto it : *split_settings) {
    if (!SettingShouldApplyToPrefs(it.first)) {
      continue;
    }

    settings.Set(it.first, it.second.Clone());
  }

  return settings;
}

void FamilyLinkSettingsService::InformSubscribers() {
  SCOPED_CRASH_KEY_STRING32("SupervisedUser", "RaceInFLSSShutdown",
                            "InInformSubscribers");

  if (!IsReady()) {
    return;
  }

  base::DictValue settings = GetSettingsWithDefault();

  // This check prevents re-emitting the same settings, including empty
  // settings. Main scenario is when this service is inactive but receives new
  // settings from its backends. Then GetSettingsWithDefault() short-circuits
  // and always returns empty settings. Re-emitting empty settings could clear
  // the supervised user prefs store that can contain settings from device
  // parental controls. Note: new subscribers always receive the current
  // settings at least once.
  if (last_notified_settings_.has_value() &&
      settings == *last_notified_settings_) {
    return;
  }

  last_notified_settings_ = settings.Clone();
  settings_callback_list_.Notify(std::move(settings));
}

WebFilterType FamilyLinkSettingsService::GetWebFilterType() const {
  // When the service is inactive or failed to initialize, we consider the web
  // filter to be disabled to not interfere with regular browsing experience.
  // Otherwise, we try to infer the filter type from the settings: block
  // always takes precedence regardless the safe sites setting. Then, safe
  // sites value determines the remaining allow or try-to-block verdict.

  // LINT.IfChange(GetWebFilterType)
  if (!active_ || initialization_failed_) {
    return WebFilterType::kDisabled;
  }

  base::DictValue settings = GetSettingsWithDefault();
  if (GetDefaultFilteringBehavior(settings) == FilteringBehavior::kBlock) {
    return WebFilterType::kCertainSites;
  }
  return IsSafeSitesEnabled(settings) ? WebFilterType::kTryToBlockMatureSites
                                      : WebFilterType::kAllowAllSites;
  // LINT.ThenChange(//components/supervised_user/core/browser/supervised_user_url_filter.cc:GetWebFilterType)
}

FilteringBehavior FamilyLinkSettingsService::GetDefaultFilteringBehavior()
    const {
  return GetDefaultFilteringBehavior(GetSettingsWithDefault());
}

FilteringBehavior FamilyLinkSettingsService::GetDefaultFilteringBehavior(
    const base::DictValue& settings) const {
  // The default value for the default filtering behavior is "allow",
  // including malformed data from remote.
  int value = settings.FindInt(kContentPackDefaultFilteringBehavior)
                  .value_or(static_cast<int>(FilteringBehavior::kAllow));

  // Only explicit match renders as `FilteringBehavior::kBlock`.
  if (value == static_cast<int>(FilteringBehavior::kBlock)) {
    return FilteringBehavior::kBlock;
  }
  // All other values are equivalent to `FilteringBehavior::kAllow` (do not
  // CHECK value, it comes from external system; default instead).
  return FilteringBehavior::kAllow;
}

bool FamilyLinkSettingsService::IsSafeSitesEnabled(
    const base::DictValue& settings) const {
  // In Family Link, safe sites setting defaults to true.
  return settings.FindBool(kSafeSitesEnabled).value_or(true);
}

FamilyLinkSettingsService::HostExceptions
FamilyLinkSettingsService::GetHostExceptions() const {
  const base::DictValue defaults = GetSettingsWithDefault();
  const base::DictValue* manual_behavior_hosts =
      defaults.FindDict(kContentPackManualBehaviorHosts);
  if (!manual_behavior_hosts) {
    return {};
  }

  HostExceptions host_exceptions;
  for (const auto&& [host, value] : *manual_behavior_hosts) {
    if (value.GetIfBool().value_or(false)) {
      host_exceptions.allowed_hosts.insert(host);
    } else {
      host_exceptions.blocked_hosts.insert(host);
    }
  }

  return host_exceptions;
}

FamilyLinkSettingsService::UrlExceptions
FamilyLinkSettingsService::GetUrlExceptions() const {
  const base::DictValue defaults = GetSettingsWithDefault();
  const base::DictValue* manual_behavior_urls =
      defaults.FindDict(kContentPackManualBehaviorURLs);
  if (!manual_behavior_urls) {
    return {};
  }

  UrlExceptions url_exceptions;
  for (const auto&& [url, value] : *manual_behavior_urls) {
    url_exceptions.emplace(GURL(url), value.GetIfBool().value_or(false));
  }
  return url_exceptions;
}

FamilyLinkSettingsService::HostExceptions::HostExceptions() = default;
FamilyLinkSettingsService::HostExceptions::~HostExceptions() = default;
FamilyLinkSettingsService::HostExceptions::HostExceptions(
    const HostExceptions& other) = default;
FamilyLinkSettingsService::HostExceptions&
FamilyLinkSettingsService::HostExceptions::operator=(
    const HostExceptions& other) = default;
}  // namespace supervised_user
