// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_service.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/prefs/value_map_pref_store.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/prefs/android/pref_service_android.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace pref_service_util {
void GetAllDottedPaths(std::string_view prefix,
                       const base::Value::Dict& dict,
                       std::vector<std::string>& paths) {
  for (const auto pair : dict) {
    std::string path;
    if (prefix.empty()) {
      path = pair.first;
    } else {
      path = base::StrCat({prefix, ".", pair.first});
    }

    if (pair.second.is_dict()) {
      GetAllDottedPaths(path, pair.second.GetDict(), paths);
    } else {
      paths.push_back(path);
    }
  }
}

void GetAllDottedPaths(const base::Value::Dict& dict,
                       std::vector<std::string>& paths) {
  GetAllDottedPaths("", dict, paths);
}
}  // namespace pref_service_util
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

PrefService::PersistentPrefStoreLoadingObserver::
    PersistentPrefStoreLoadingObserver(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
}

void PrefService::PersistentPrefStoreLoadingObserver::OnInitializationCompleted(
    bool succeeded) {
  pref_service_->CheckPrefsLoaded();
}

PrefService::PrefService(
    std::unique_ptr<PrefNotifierImpl> pref_notifier,
    std::unique_ptr<PrefValueStore> pref_value_store,
    scoped_refptr<PersistentPrefStore> user_prefs,
    scoped_refptr<PersistentPrefStore> standalone_browser_prefs,
    scoped_refptr<PrefRegistry> pref_registry,
    base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
        read_error_callback,
    bool async)
    : pref_notifier_(std::move(pref_notifier)),
      pref_value_store_(std::move(pref_value_store)),
      user_pref_store_(std::move(user_prefs)),
      standalone_browser_pref_store_(std::move(standalone_browser_prefs)),
      read_error_callback_(std::move(read_error_callback)),
      pref_registry_(std::move(pref_registry)),
      pref_store_observer_(
          std::make_unique<PrefService::PersistentPrefStoreLoadingObserver>(
              this)) {
  pref_notifier_->SetPrefService(this);

  DCHECK(pref_registry_);
  DCHECK(pref_value_store_);

  InitFromStorage(async);
}

PrefService::~PrefService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Remove observers. This could be necessary if this service is destroyed
  // before the prefs are fully loaded.
  user_pref_store_->RemoveObserver(pref_store_observer_.get());
  if (standalone_browser_pref_store_) {
    standalone_browser_pref_store_->RemoveObserver(pref_store_observer_.get());
  }

  // TODO(crbug.com/942491, 946668, 945772) The following code collects
  // augments stack dumps created by ~PrefNotifierImpl() with information
  // whether the profile owning the PrefService is an incognito profile.
  // Delete this, once the bugs are closed.
  const bool is_incognito_profile = user_pref_store_->IsInMemoryPrefStore();
  base::debug::Alias(&is_incognito_profile);
  // Export value of is_incognito_profile to a string so that `grep`
  // is a sufficient tool to analyze crashdumps.
  char is_incognito_profile_string[32];
  strncpy(is_incognito_profile_string,
          is_incognito_profile ? "is_incognito: yes" : "is_incognito: no",
          sizeof(is_incognito_profile_string));
  base::debug::Alias(&is_incognito_profile_string);
}

void PrefService::InitFromStorage(bool async) {
  if (!async) {
    if (!user_pref_store_->IsInitializationComplete()) {
      user_pref_store_->ReadPrefs();
    }
    if (standalone_browser_pref_store_ &&
        !standalone_browser_pref_store_->IsInitializationComplete()) {
      standalone_browser_pref_store_->ReadPrefs();
    }
    CheckPrefsLoaded();
    return;
  }

  CheckPrefsLoaded();

  if (!user_pref_store_->IsInitializationComplete()) {
    user_pref_store_->AddObserver(pref_store_observer_.get());
    user_pref_store_->ReadPrefsAsync(nullptr);
  }

  if (standalone_browser_pref_store_ &&
      !standalone_browser_pref_store_->IsInitializationComplete()) {
    standalone_browser_pref_store_->AddObserver(pref_store_observer_.get());
    standalone_browser_pref_store_->ReadPrefsAsync(nullptr);
  }
}

void PrefService::CheckPrefsLoaded() {
  if (!(user_pref_store_->IsInitializationComplete() &&
        (!standalone_browser_pref_store_ ||
         standalone_browser_pref_store_->IsInitializationComplete()))) {
    // Not done initializing both prefstores.
    return;
  }

  user_pref_store_->RemoveObserver(pref_store_observer_.get());
  if (standalone_browser_pref_store_) {
    standalone_browser_pref_store_->RemoveObserver(pref_store_observer_.get());
  }

  // Both prefstores are initialized, get the read errors.
  PersistentPrefStore::PrefReadError user_store_error =
      user_pref_store_->GetReadError();
  if (!standalone_browser_pref_store_) {
    read_error_callback_.Run(user_store_error);
    return;
  }
  PersistentPrefStore::PrefReadError standalone_browser_store_error =
      standalone_browser_pref_store_->GetReadError();

  // If both stores have the same error (or no error), run the callback with
  // either one. This avoids double-reporting (either way prefs weren't
  // successfully fully loaded)
  if (user_store_error == standalone_browser_store_error) {
    read_error_callback_.Run(user_store_error);
  } else if (user_store_error == PersistentPrefStore::PREF_READ_ERROR_NONE ||
             user_store_error == PersistentPrefStore::PREF_READ_ERROR_NO_FILE) {
    // Prefer to report the standalone_browser_pref_store error if the
    // user_pref_store error is not significant.
    read_error_callback_.Run(standalone_browser_store_error);
  } else {
    // Either the user_pref_store error is significant, or
    // both stores failed to load but for different reasons.
    // The user_store error is more significant in essentially all cases,
    // so prefer to report that.
    read_error_callback_.Run(user_store_error);
  }
}

void PrefService::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_pref_store_->CommitPendingWrite(std::move(reply_callback),
                                       std::move(synchronous_done_callback));
}

void PrefService::SchedulePendingLossyWrites() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_pref_store_->SchedulePendingLossyWrites();
}

bool PrefService::GetBoolean(std::string_view path) const {
  return GetValue(path).GetBool();
}

int PrefService::GetInteger(std::string_view path) const {
  return GetValue(path).GetInt();
}

double PrefService::GetDouble(std::string_view path) const {
  return GetValue(path).GetDouble();
}

const std::string& PrefService::GetString(std::string_view path) const {
  return GetValue(path).GetString();
}

base::FilePath PrefService::GetFilePath(std::string_view path) const {
  const base::Value& value = GetValue(path);
  std::optional<base::FilePath> result = base::ValueToFilePath(value);
  DCHECK(result);
  return *result;
}

bool PrefService::HasPrefPath(std::string_view path) const {
  const Preference* pref = FindPreference(path);
  return pref && !pref->IsDefaultValue();
}

void PrefService::IteratePreferenceValues(
    base::RepeatingCallback<void(const std::string& key,
                                 const base::Value& value)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& it : *pref_registry_)
    callback.Run(it.first, *GetPreferenceValue(it.first));
}

base::Value::Dict PrefService::GetPreferenceValues(
    IncludeDefaults include_defaults) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value::Dict out;
  for (const auto& it : *pref_registry_) {
    if (include_defaults == INCLUDE_DEFAULTS) {
      out.SetByDottedPath(it.first, GetPreferenceValue(it.first)->Clone());
    } else {
      const Preference* pref = FindPreference(it.first);
      if (pref->IsDefaultValue()) {
        continue;
      }
      out.SetByDottedPath(it.first, pref->GetValue()->Clone());
    }
  }
  return out;
}

std::vector<PrefService::PreferenceValueAndStore>
PrefService::GetPreferencesValueAndStore() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PreferenceValueAndStore> result;
  for (const auto& it : *pref_registry_) {
    auto* preference = FindPreference(it.first);
    CHECK(preference);
    PreferenceValueAndStore pref_data{
        it.first, preference->GetValue()->Clone(),
        pref_value_store_->ControllingPrefStoreForPref(it.first)};
    result.emplace_back(std::move(pref_data));
  }
  return result;
}

const PrefService::Preference* PrefService::FindPreference(
    std::string_view path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = prefs_map_.find(path);
  if (it != prefs_map_.end())
    return &(it->second);
  const base::Value* default_value = nullptr;
  if (!pref_registry_->defaults()->GetValue(path, &default_value)) {
    return nullptr;
  }
  it = prefs_map_
           .insert(std::make_pair(
               std::string(path),
               Preference(this, std::string(path), default_value->type())))
           .first;
  return &(it->second);
}

bool PrefService::ReadOnly() const {
  return user_pref_store_->ReadOnly();
}

PrefService::PrefInitializationStatus PrefService::GetInitializationStatus()
    const {
  if (!user_pref_store_->IsInitializationComplete())
    return INITIALIZATION_STATUS_WAITING;

  switch (user_pref_store_->GetReadError()) {
    case PersistentPrefStore::PREF_READ_ERROR_NONE:
      return INITIALIZATION_STATUS_SUCCESS;
    case PersistentPrefStore::PREF_READ_ERROR_NO_FILE:
      return INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
    default:
      return INITIALIZATION_STATUS_ERROR;
  }
}

PrefService::PrefInitializationStatus
PrefService::GetAllPrefStoresInitializationStatus() const {
  if (!pref_value_store_->IsInitializationComplete())
    return INITIALIZATION_STATUS_WAITING;

  return GetInitializationStatus();
}

bool PrefService::IsManagedPreference(std::string_view path) const {
  const Preference* pref = FindPreference(path);
  return pref && pref->IsManaged();
}

bool PrefService::IsPreferenceManagedByCustodian(std::string_view path) const {
  const Preference* pref = FindPreference(path);
  return pref && pref->IsManagedByCustodian();
}

bool PrefService::IsUserModifiablePreference(std::string_view path) const {
  const Preference* pref = FindPreference(path);
  return pref && pref->IsUserModifiable();
}

const base::Value& PrefService::GetValue(std::string_view path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return *GetPreferenceValue(path);
}

const base::Value::Dict& PrefService::GetDict(std::string_view path) const {
  const base::Value& value = GetValue(path);
  return value.GetDict();
}

const base::Value::List& PrefService::GetList(std::string_view path) const {
  const base::Value& value = GetValue(path);
  return value.GetList();
}

const base::Value* PrefService::GetUserPrefValue(std::string_view path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED_IN_MIGRATION() << "Trying to get an unregistered pref: " << path;
    return nullptr;
  }

  // Look for an existing preference in the user store. If it doesn't
  // exist, return NULL.
  base::Value* value = nullptr;
  if (!user_pref_store_->GetMutableValue(path, &value))
    return nullptr;

  if (value->type() != pref->GetType()) {
    DUMP_WILL_BE_NOTREACHED()
        << "Pref value type doesn't match registered type.";
    return nullptr;
  }

  return value;
}

void PrefService::SetDefaultPrefValue(std::string_view path,
                                      base::Value value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_registry_->SetDefaultPrefValue(path, std::move(value));
}

const base::Value* PrefService::GetDefaultPrefValue(
    std::string_view path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Lookup the preference in the default store.
  const base::Value* value = nullptr;
  bool has_value = pref_registry_->defaults()->GetValue(path, &value);
  DCHECK(has_value) << "Default value missing for pref: " << path;
  return value;
}

void PrefService::AddPrefObserver(std::string_view path, PrefObserver* obs) {
  pref_notifier_->AddPrefObserver(path, obs);
}

void PrefService::RemovePrefObserver(std::string_view path, PrefObserver* obs) {
  pref_notifier_->RemovePrefObserver(path, obs);
}

void PrefService::AddPrefInitObserver(base::OnceCallback<void(bool)> obs) {
  pref_notifier_->AddInitObserver(std::move(obs));
}

PrefRegistry* PrefService::DeprecatedGetPrefRegistry() {
  return pref_registry_.get();
}

void PrefService::ClearPref(std::string_view path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED_IN_MIGRATION()
        << "Trying to clear an unregistered pref: " << path;
    return;
  }
  user_pref_store_->RemoveValue(path, GetWriteFlags(pref));
}

void PrefService::ClearPrefsWithPrefixSilently(std::string_view prefix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_pref_store_->RemoveValuesByPrefixSilently(prefix);
}

void PrefService::OnStoreDeletionFromDisk() {
  user_pref_store_->OnStoreDeletionFromDisk();
}

void PrefService::AddPrefObserverAllPrefs(PrefObserver* obs) {
  pref_notifier_->AddPrefObserverAllPrefs(obs);
}

void PrefService::RemovePrefObserverAllPrefs(PrefObserver* obs) {
  pref_notifier_->RemovePrefObserverAllPrefs(obs);
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> PrefService::GetJavaObject() {
  if (!pref_service_android_) {
    pref_service_android_ = std::make_unique<PrefServiceAndroid>(this);
  }
  return pref_service_android_->GetJavaObject();
}
#endif

void PrefService::Set(std::string_view path, const base::Value& value) {
  SetUserPrefValue(path, value.Clone());
}

void PrefService::SetBoolean(std::string_view path, bool value) {
  SetUserPrefValue(path, base::Value(value));
}

void PrefService::SetInteger(std::string_view path, int value) {
  SetUserPrefValue(path, base::Value(value));
}

void PrefService::SetDouble(std::string_view path, double value) {
  SetUserPrefValue(path, base::Value(value));
}

void PrefService::SetString(std::string_view path, std::string_view value) {
  SetUserPrefValue(path, base::Value(value));
}

void PrefService::SetDict(std::string_view path, base::Value::Dict dict) {
  SetUserPrefValue(path, base::Value(std::move(dict)));
}

void PrefService::SetList(std::string_view path, base::Value::List list) {
  SetUserPrefValue(path, base::Value(std::move(list)));
}

void PrefService::SetFilePath(std::string_view path,
                              const base::FilePath& value) {
  SetUserPrefValue(path, base::FilePathToValue(value));
}

void PrefService::SetInt64(std::string_view path, int64_t value) {
  SetUserPrefValue(path, base::Int64ToValue(value));
}

int64_t PrefService::GetInt64(std::string_view path) const {
  const base::Value& value = GetValue(path);
  std::optional<int64_t> integer = base::ValueToInt64(value);
  DCHECK(integer);
  return integer.value_or(0);
}

void PrefService::SetUint64(std::string_view path, uint64_t value) {
  SetUserPrefValue(path, base::Value(base::NumberToString(value)));
}

uint64_t PrefService::GetUint64(std::string_view path) const {
  const base::Value& value = GetValue(path);
  if (!value.is_string())
    return 0;

  uint64_t result;
  base::StringToUint64(value.GetString(), &result);
  return result;
}

void PrefService::SetTime(std::string_view path, base::Time value) {
  SetUserPrefValue(path, base::TimeToValue(value));
}

base::Time PrefService::GetTime(std::string_view path) const {
  const base::Value& value = GetValue(path);
  std::optional<base::Time> time = base::ValueToTime(value);
  DCHECK(time);
  return time.value_or(base::Time());
}

void PrefService::SetTimeDelta(std::string_view path, base::TimeDelta value) {
  SetUserPrefValue(path, base::TimeDeltaToValue(value));
}

base::TimeDelta PrefService::GetTimeDelta(std::string_view path) const {
  const base::Value& value = GetValue(path);
  std::optional<base::TimeDelta> time_delta = base::ValueToTimeDelta(value);
  DCHECK(time_delta);
  return time_delta.value_or(base::TimeDelta());
}

base::Value* PrefService::GetMutableUserPref(std::string_view path,
                                             base::Value::Type type) {
  CHECK(type == base::Value::Type::DICT || type == base::Value::Type::LIST);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    DUMP_WILL_BE_NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return nullptr;
  }
  if (pref->GetType() != type) {
    NOTREACHED_IN_MIGRATION() << "Wrong type for GetMutableValue: " << path;
    return nullptr;
  }

  // Look for an existing preference in the user store. Return it in case it
  // exists and has the correct type.
  base::Value* value = nullptr;
  if (user_pref_store_->GetMutableValue(path, &value) &&
      value->type() == type) {
    return value;
  }

  // If no user preference of the correct type exists, clone default value.
  const base::Value* default_value = nullptr;
  pref_registry_->defaults()->GetValue(path, &default_value);
  DCHECK_EQ(default_value->type(), type);
  user_pref_store_->SetValueSilently(path, default_value->Clone(),
                                     GetWriteFlags(pref));
  user_pref_store_->GetMutableValue(path, &value);
  return value;
}

void PrefService::ReportUserPrefChanged(const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_pref_store_->ReportValueChanged(key, GetWriteFlags(FindPreference(key)));
}

void PrefService::ReportUserPrefChanged(
    const std::string& key,
    std::set<std::vector<std::string>> path_components) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_pref_store_->ReportSubValuesChanged(key, std::move(path_components),
                                           GetWriteFlags(FindPreference(key)));
}

void PrefService::SetUserPrefValue(std::string_view path,
                                   base::Value new_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    DUMP_WILL_BE_NOTREACHED()
        << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->GetType() != new_value.type()) {
    NOTREACHED_IN_MIGRATION()
        << "Trying to set pref " << path << " of type " << pref->GetType()
        << " to value of type " << new_value.type();
    return;
  }

  user_pref_store_->SetValue(path, std::move(new_value), GetWriteFlags(pref));
}

void PrefService::UpdateCommandLinePrefStore(PrefStore* command_line_store) {
  pref_value_store_->UpdateCommandLinePrefStore(command_line_store);
}

///////////////////////////////////////////////////////////////////////////////
// PrefService::Preference

PrefService::Preference::Preference(const PrefService* service,
                                    std::string name,
                                    base::Value::Type type)
    : name_(std::move(name)),
      type_(type),
      // Cache the registration flags at creation time to avoid multiple map
      // lookups later.
      registration_flags_(service->pref_registry_->GetRegistrationFlags(name_)),
      pref_service_(CHECK_DEREF(service)) {}

const base::Value* PrefService::Preference::GetValue() const {
  return pref_service_->GetPreferenceValue(name_);
}

const base::Value* PrefService::Preference::GetRecommendedValue() const {
  DCHECK(pref_service_->FindPreference(name_))
      << "Must register pref before getting its value";

  const base::Value* found_value = nullptr;
  if (pref_value_store()->GetRecommendedValue(name_, type_, &found_value)) {
    DCHECK(found_value->type() == type_);
    return found_value;
  }

  // The pref has no recommended value.
  return nullptr;
}

bool PrefService::Preference::IsManaged() const {
  return pref_value_store()->PrefValueInManagedStore(name_);
}

bool PrefService::Preference::IsManagedByCustodian() const {
  return pref_value_store()->PrefValueInSupervisedStore(name_);
}

bool PrefService::Preference::IsRecommended() const {
  return pref_value_store()->PrefValueFromRecommendedStore(name_);
}

bool PrefService::Preference::HasExtensionSetting() const {
  return pref_value_store()->PrefValueInExtensionStore(name_);
}

bool PrefService::Preference::HasUserSetting() const {
  return pref_value_store()->PrefValueInUserStore(name_);
}

bool PrefService::Preference::IsExtensionControlled() const {
  return pref_value_store()->PrefValueFromExtensionStore(name_);
}

bool PrefService::Preference::IsUserControlled() const {
  return pref_value_store()->PrefValueFromUserStore(name_);
}

bool PrefService::Preference::IsDefaultValue() const {
  return pref_value_store()->PrefValueFromDefaultStore(name_);
}

bool PrefService::Preference::IsUserModifiable() const {
  return pref_value_store()->PrefValueUserModifiable(name_);
}

bool PrefService::Preference::IsExtensionModifiable() const {
  return pref_value_store()->PrefValueExtensionModifiable(name_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool PrefService::Preference::IsStandaloneBrowserControlled() const {
  return pref_value_store()->PrefValueFromStandaloneBrowserStore(name_);
}

bool PrefService::Preference::IsStandaloneBrowserModifiable() const {
  return pref_value_store()->PrefValueStandaloneBrowserModifiable(name_);
}
#endif

const base::Value* PrefService::GetPreferenceValue(
    std::string_view path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* default_value = nullptr;
  CHECK(pref_registry_->defaults()->GetValue(path, &default_value))
      << "Trying to access an unregistered pref: " << path;
  CHECK(default_value);
  const base::Value::Type default_type = default_value->type();

  const base::Value* found_value = nullptr;
  // GetValue shouldn't fail because every registered preference has at least a
  // default value.
  CHECK(pref_value_store_->GetValue(path, default_type, &found_value));
  CHECK(found_value);
  // The type is expected to match here thanks to a verification in
  // PrefValueStore::GetValueFromStoreWithType which discards polluted values
  // (and we should at least get a matching type from the default store if no
  // other store has a valid value+type).
  CHECK_EQ(found_value->type(), default_type);
  return found_value;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PrefService::SetStandaloneBrowserPref(std::string_view path,
                                           const base::Value& value) {
  if (!standalone_browser_pref_store_) {
    LOG(WARNING) << "Failure to set value of " << path
                 << " in standalone browser store";
    return;
  }
  standalone_browser_pref_store_->SetValue(
      path, value.Clone(), WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

void PrefService::RemoveStandaloneBrowserPref(std::string_view path) {
  if (!standalone_browser_pref_store_) {
    LOG(WARNING) << "Failure to remove value of " << path
                 << " in standalone browser store";
    return;
  }
  standalone_browser_pref_store_->RemoveValue(
      path, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

void PrefService::RemoveAllStandaloneBrowserPrefs() {
  if (!standalone_browser_pref_store_) {
    LOG(WARNING) << "standalone_browser_pref_store_ is null";
    return;
  }

  std::vector<std::string> paths;
  pref_service_util::GetAllDottedPaths(
      standalone_browser_pref_store_->GetValues(), paths);

  for (const std::string& path : paths) {
    standalone_browser_pref_store_->RemoveValue(
        path, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}
#endif

// static
uint32_t PrefService::GetWriteFlags(const PrefService::Preference* pref) {
  uint32_t write_flags = WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS;

  if (!pref) {
    return write_flags;
  }

  if (pref->registration_flags() & PrefRegistry::LOSSY_PREF) {
    write_flags |= WriteablePrefStore::LOSSY_PREF_WRITE_FLAG;
  }
  return write_flags;
}
