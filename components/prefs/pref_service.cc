// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_service.h"

#include <algorithm>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/util/values/values_util.h"
#include "base/value_conversions.h"
#include "build/build_config.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry.h"

namespace {

class ReadErrorHandler : public PersistentPrefStore::ReadErrorDelegate {
 public:
  using ErrorCallback =
      base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>;
  explicit ReadErrorHandler(ErrorCallback cb) : callback_(cb) {}

  void OnError(PersistentPrefStore::PrefReadError error) override {
    callback_.Run(error);
  }

 private:
  ErrorCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ReadErrorHandler);
};

// Returns the WriteablePrefStore::PrefWriteFlags for the pref with the given
// |path|.
uint32_t GetWriteFlags(const PrefService::Preference* pref) {
  uint32_t write_flags = WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS;

  if (!pref)
    return write_flags;

  if (pref->registration_flags() & PrefRegistry::LOSSY_PREF)
    write_flags |= WriteablePrefStore::LOSSY_PREF_WRITE_FLAG;
  return write_flags;
}

// For prefs names in |pref_store| that are not presented in |pref_changed_map|,
// check if their values differ from those in pref_service->FindPreference() and
// add the result into |pref_changed_map|.
void CheckForNewPrefChangesInPrefStore(
    std::map<std::string, bool>* pref_changed_map,
    PrefStore* pref_store,
    PrefService* pref_service) {
  if (!pref_store)
    return;
  auto values = pref_store->GetValues();
  for (const auto& item : values->DictItems()) {
    // If the key already presents, skip it as a store with higher precedence
    // already sets the entry.
    if (pref_changed_map->find(item.first) != pref_changed_map->end())
      continue;
    const PrefService::Preference* pref =
        pref_service->FindPreference(item.first);
    if (!pref)
      continue;
    pref_changed_map->emplace(item.first, *(pref->GetValue()) != item.second);
  }
}

}  // namespace

PrefService::PrefService(
    std::unique_ptr<PrefNotifierImpl> pref_notifier,
    std::unique_ptr<PrefValueStore> pref_value_store,
    scoped_refptr<PersistentPrefStore> user_prefs,
    scoped_refptr<PrefRegistry> pref_registry,
    base::RepeatingCallback<void(PersistentPrefStore::PrefReadError)>
        read_error_callback,
    bool async)
    : pref_notifier_(std::move(pref_notifier)),
      pref_value_store_(std::move(pref_value_store)),
      user_pref_store_(std::move(user_prefs)),
      read_error_callback_(std::move(read_error_callback)),
      pref_registry_(std::move(pref_registry)) {
  pref_notifier_->SetPrefService(this);

  DCHECK(pref_registry_);
  DCHECK(pref_value_store_);

  InitFromStorage(async);
}

PrefService::~PrefService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  if (user_pref_store_->IsInitializationComplete()) {
    read_error_callback_.Run(user_pref_store_->GetReadError());
  } else if (!async) {
    read_error_callback_.Run(user_pref_store_->ReadPrefs());
  } else {
    // Guarantee that initialization happens after this function returned.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&PersistentPrefStore::ReadPrefsAsync, user_pref_store_,
                       new ReadErrorHandler(read_error_callback_)));
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

bool PrefService::GetBoolean(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool result = false;

  const base::Value* value = GetPreferenceValueChecked(path);
  if (!value)
    return result;
  bool rv = value->GetAsBoolean(&result);
  DCHECK(rv);
  return result;
}

int PrefService::GetInteger(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int result = 0;

  const base::Value* value = GetPreferenceValueChecked(path);
  if (!value)
    return result;
  bool rv = value->GetAsInteger(&result);
  DCHECK(rv);
  return result;
}

double PrefService::GetDouble(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  double result = 0.0;

  const base::Value* value = GetPreferenceValueChecked(path);
  if (!value)
    return result;
  bool rv = value->GetAsDouble(&result);
  DCHECK(rv);
  return result;
}

std::string PrefService::GetString(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string result;

  const base::Value* value = GetPreferenceValueChecked(path);
  if (!value)
    return result;
  bool rv = value->GetAsString(&result);
  DCHECK(rv);
  return result;
}

base::FilePath PrefService::GetFilePath(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath result;

  const base::Value* value = GetPreferenceValueChecked(path);
  if (!value)
    return base::FilePath(result);
  bool rv = base::GetValueAsFilePath(*value, &result);
  DCHECK(rv);
  return result;
}

bool PrefService::HasPrefPath(const std::string& path) const {
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

std::unique_ptr<base::DictionaryValue> PrefService::GetPreferenceValues(
    IncludeDefaults include_defaults) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<base::DictionaryValue> out(new base::DictionaryValue);
  for (const auto& it : *pref_registry_) {
    if (include_defaults == INCLUDE_DEFAULTS) {
      out->Set(it.first, GetPreferenceValue(it.first)->CreateDeepCopy());
    } else {
      const Preference* pref = FindPreference(it.first);
      if (pref->IsDefaultValue())
        continue;
      out->Set(it.first, pref->GetValue()->CreateDeepCopy());
    }
  }
  return out;
}

const PrefService::Preference* PrefService::FindPreference(
    const std::string& pref_name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = prefs_map_.find(pref_name);
  if (it != prefs_map_.end())
    return &(it->second);
  const base::Value* default_value = nullptr;
  if (!pref_registry_->defaults()->GetValue(pref_name, &default_value))
    return nullptr;
  it = prefs_map_
           .insert(std::make_pair(
               pref_name, Preference(this, pref_name, default_value->type())))
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

bool PrefService::IsManagedPreference(const std::string& pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  return pref && pref->IsManaged();
}

bool PrefService::IsPreferenceManagedByCustodian(
    const std::string& pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  return pref && pref->IsManagedByCustodian();
}

bool PrefService::IsUserModifiablePreference(
    const std::string& pref_name) const {
  const Preference* pref = FindPreference(pref_name);
  return pref && pref->IsUserModifiable();
}

const base::Value* PrefService::Get(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value = GetPreferenceValueChecked(path);
  if (!value)
    return nullptr;
  return value;
}

const base::DictionaryValue* PrefService::GetDictionary(
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value = GetPreferenceValueChecked(path);
  if (!value)
    return nullptr;
  if (value->type() != base::Value::Type::DICTIONARY) {
    NOTREACHED();
    return nullptr;
  }
  return static_cast<const base::DictionaryValue*>(value);
}

const base::Value* PrefService::GetUserPrefValue(
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return nullptr;
  }

  // Look for an existing preference in the user store. If it doesn't
  // exist, return NULL.
  base::Value* value = nullptr;
  if (!user_pref_store_->GetMutableValue(path, &value))
    return nullptr;

  if (value->type() != pref->GetType()) {
    NOTREACHED() << "Pref value type doesn't match registered type.";
    return nullptr;
  }

  return value;
}

void PrefService::SetDefaultPrefValue(const std::string& path,
                                      base::Value value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_registry_->SetDefaultPrefValue(path, std::move(value));
}

const base::Value* PrefService::GetDefaultPrefValue(
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Lookup the preference in the default store.
  const base::Value* value = nullptr;
  bool has_value = pref_registry_->defaults()->GetValue(path, &value);
  DCHECK(has_value) << "Default value missing for pref: " << path;
  return value;
}

const base::ListValue* PrefService::GetList(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value = GetPreferenceValueChecked(path);
  if (!value)
    return nullptr;
  if (value->type() != base::Value::Type::LIST) {
    NOTREACHED();
    return nullptr;
  }
  return static_cast<const base::ListValue*>(value);
}

void PrefService::AddPrefObserver(const std::string& path, PrefObserver* obs) {
  pref_notifier_->AddPrefObserver(path, obs);
}

void PrefService::RemovePrefObserver(const std::string& path,
                                     PrefObserver* obs) {
  pref_notifier_->RemovePrefObserver(path, obs);
}

void PrefService::AddPrefInitObserver(base::OnceCallback<void(bool)> obs) {
  pref_notifier_->AddInitObserver(std::move(obs));
}

PrefRegistry* PrefService::DeprecatedGetPrefRegistry() {
  return pref_registry_.get();
}

void PrefService::ClearPref(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to clear an unregistered pref: " << path;
    return;
  }
  user_pref_store_->RemoveValue(path, GetWriteFlags(pref));
}

void PrefService::ClearMutableValues() {
  user_pref_store_->ClearMutableValues();
}

void PrefService::OnStoreDeletionFromDisk() {
  user_pref_store_->OnStoreDeletionFromDisk();
}

void PrefService::ChangePrefValueStore(
    PrefStore* managed_prefs,
    PrefStore* supervised_user_prefs,
    PrefStore* extension_prefs,
    PrefStore* recommended_prefs,
    std::unique_ptr<PrefValueStore::Delegate> delegate) {
  // Only adding new pref stores are supported.
  DCHECK(!pref_value_store_->HasPrefStore(PrefValueStore::MANAGED_STORE) ||
         !managed_prefs);
  DCHECK(
      !pref_value_store_->HasPrefStore(PrefValueStore::SUPERVISED_USER_STORE) ||
      !supervised_user_prefs);
  DCHECK(!pref_value_store_->HasPrefStore(PrefValueStore::EXTENSION_STORE) ||
         !extension_prefs);
  DCHECK(!pref_value_store_->HasPrefStore(PrefValueStore::RECOMMENDED_STORE) ||
         !recommended_prefs);

  // If some of the stores are already initialized, check for pref value changes
  // according to store precedence.
  std::map<std::string, bool> pref_changed_map;
  CheckForNewPrefChangesInPrefStore(&pref_changed_map, managed_prefs, this);
  CheckForNewPrefChangesInPrefStore(&pref_changed_map, supervised_user_prefs,
                                    this);
  CheckForNewPrefChangesInPrefStore(&pref_changed_map, extension_prefs, this);
  CheckForNewPrefChangesInPrefStore(&pref_changed_map, recommended_prefs, this);

  pref_value_store_ = pref_value_store_->CloneAndSpecialize(
      managed_prefs, supervised_user_prefs, extension_prefs,
      nullptr /* command_line_prefs */, nullptr /* user_prefs */,
      recommended_prefs, nullptr /* default_prefs */, pref_notifier_.get(),
      std::move(delegate));

  // Notify |pref_notifier_| on all changed values.
  for (const auto& kv : pref_changed_map) {
    if (kv.second)
      pref_notifier_.get()->OnPreferenceChanged(kv.first);
  }
}

void PrefService::AddPrefObserverAllPrefs(PrefObserver* obs) {
  pref_notifier_->AddPrefObserverAllPrefs(obs);
}

void PrefService::RemovePrefObserverAllPrefs(PrefObserver* obs) {
  pref_notifier_->RemovePrefObserverAllPrefs(obs);
}

void PrefService::Set(const std::string& path, const base::Value& value) {
  SetUserPrefValue(path, value.Clone());
}

void PrefService::SetBoolean(const std::string& path, bool value) {
  SetUserPrefValue(path, base::Value(value));
}

void PrefService::SetInteger(const std::string& path, int value) {
  SetUserPrefValue(path, base::Value(value));
}

void PrefService::SetDouble(const std::string& path, double value) {
  SetUserPrefValue(path, base::Value(value));
}

void PrefService::SetString(const std::string& path, const std::string& value) {
  SetUserPrefValue(path, base::Value(value));
}

void PrefService::SetFilePath(const std::string& path,
                              const base::FilePath& value) {
  SetUserPrefValue(path, base::CreateFilePathValue(value));
}

void PrefService::SetInt64(const std::string& path, int64_t value) {
  SetUserPrefValue(path, util::Int64ToValue(value));
}

int64_t PrefService::GetInt64(const std::string& path) const {
  const base::Value* value = GetPreferenceValueChecked(path);
  base::Optional<int64_t> integer = util::ValueToInt64(value);
  DCHECK(integer);
  return integer.value_or(0);
}

void PrefService::SetUint64(const std::string& path, uint64_t value) {
  SetUserPrefValue(path, base::Value(base::NumberToString(value)));
}

uint64_t PrefService::GetUint64(const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* value = GetPreferenceValueChecked(path);
  if (!value)
    return 0;
  std::string result("0");
  bool rv = value->GetAsString(&result);
  DCHECK(rv);

  uint64_t val;
  base::StringToUint64(result, &val);
  return val;
}

void PrefService::SetTime(const std::string& path, base::Time value) {
  SetUserPrefValue(path, util::TimeToValue(value));
}

base::Time PrefService::GetTime(const std::string& path) const {
  const base::Value* value = GetPreferenceValueChecked(path);
  base::Optional<base::Time> time = util::ValueToTime(value);
  DCHECK(time);
  return time.value_or(base::Time());
}

void PrefService::SetTimeDelta(const std::string& path, base::TimeDelta value) {
  SetUserPrefValue(path, util::TimeDeltaToValue(value));
}

base::TimeDelta PrefService::GetTimeDelta(const std::string& path) const {
  const base::Value* value = GetPreferenceValueChecked(path);
  base::Optional<base::TimeDelta> time_delta = util::ValueToTimeDelta(value);
  DCHECK(time_delta);
  return time_delta.value_or(base::TimeDelta());
}

base::Value* PrefService::GetMutableUserPref(const std::string& path,
                                             base::Value::Type type) {
  CHECK(type == base::Value::Type::DICTIONARY ||
        type == base::Value::Type::LIST);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return nullptr;
  }
  if (pref->GetType() != type) {
    NOTREACHED() << "Wrong type for GetMutableValue: " << path;
    return nullptr;
  }

  // Look for an existing preference in the user store. Return it in case it
  // exists and has the correct type.
  base::Value* value = nullptr;
  if (user_pref_store_->GetMutableValue(path, &value) &&
      value->type() == type) {
    return value;
  }

  // TODO(crbug.com/859477): Remove once root cause has been found.
  if (value && value->type() != type) {
    DEBUG_ALIAS_FOR_CSTR(path_copy, path.c_str(), 1024);
    base::debug::DumpWithoutCrashing();
  }

  // If no user preference of the correct type exists, clone default value.
  const base::Value* default_value = nullptr;
  pref_registry_->defaults()->GetValue(path, &default_value);
  // TODO(crbug.com/859477): Revert to DCHECK once root cause has been found.
  if (default_value->type() != type) {
    DEBUG_ALIAS_FOR_CSTR(path_copy, path.c_str(), 1024);
    base::debug::DumpWithoutCrashing();
  }
  user_pref_store_->SetValueSilently(path, default_value->CreateDeepCopy(),
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

void PrefService::SetUserPrefValue(const std::string& path,
                                   base::Value new_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->GetType() != new_value.type()) {
    NOTREACHED() << "Trying to set pref " << path << " of type "
                 << pref->GetType() << " to value of type " << new_value.type();
    return;
  }

  user_pref_store_->SetValue(
      path, base::Value::ToUniquePtrValue(std::move(new_value)),
      GetWriteFlags(pref));
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
      pref_service_(service) {}

const base::Value* PrefService::Preference::GetValue() const {
  return pref_service_->GetPreferenceValueChecked(name_);
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

const base::Value* PrefService::GetPreferenceValue(
    const std::string& path) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(battre): This is a check for crbug.com/435208. After analyzing some
  // crash dumps it looks like the PrefService is accessed even though it has
  // been cleared already.
  CHECK(pref_registry_);
  CHECK(pref_registry_->defaults());
  CHECK(pref_value_store_);

  const base::Value* default_value = nullptr;
  if (pref_registry_->defaults()->GetValue(path, &default_value)) {
    const base::Value* found_value = nullptr;
    base::Value::Type default_type = default_value->type();
    if (pref_value_store_->GetValue(path, default_type, &found_value)) {
      DCHECK(found_value->type() == default_type);
      return found_value;
    } else {
      // Every registered preference has at least a default value.
      NOTREACHED() << "no valid value found for registered pref " << path;
    }
  }

  return nullptr;
}

const base::Value* PrefService::GetPreferenceValueChecked(
    const std::string& path) const {
  const base::Value* value = GetPreferenceValue(path);
  DCHECK(value) << "Trying to read an unregistered pref: " << path;
  return value;
}
