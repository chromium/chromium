// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/persisted_data.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "base/version.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/update_client_errors.h"

namespace update_client {

const char kPersistedDataPreference[] = "updateclientdata";
const char kLastUpdateCheckErrorPreference[] =
    "updateclientlastupdatecheckerror";
const char kLastUpdateCheckErrorCategoryPreference[] =
    "updateclientlastupdatecheckerrorcategory";
const char kLastUpdateCheckErrorExtraCode1Preference[] =
    "updateclientlastupdatecheckerrorextracode1";

namespace {

const char kThrottleUpdatesUntilPreference[] = "updateclientthrottleuntil";

class PersistedDataImpl : public PersistedData {
 public:
  // Constructs a provider using the specified |pref_service| and
  // |activity_data_service|.
  // The associated preferences are assumed to already be registered.
  // The |pref_service| and |activity_data_service| must outlive the entire
  // update_client.
  PersistedDataImpl(PrefService* pref_service,
                    std::unique_ptr<ActivityDataService> activity_data_service);
  PersistedDataImpl(const PersistedDataImpl&) = delete;
  PersistedDataImpl& operator=(const PersistedDataImpl&) = delete;

  ~PersistedDataImpl() override;

  // This is called only via update_client's RegisterUpdateClientPreferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // PersistedData overrides:
  int GetDateLastRollCall(const std::string& id) const override;
  int GetDateLastActive(const std::string& id) const override;
  std::string GetPingFreshness(const std::string& id) const override;
  void SetDateLastData(const std::vector<std::string>& ids,
                       int datenum,
                       base::OnceClosure callback) override;
  void SetDateLastActive(const std::string& id, int dla) override;
  void SetDateLastRollCall(const std::string& id, int dlrc) override;
  int GetInstallDate(const std::string& id) const override;
  void SetInstallDate(const std::string& id, int install_date) override;
  std::string GetCohort(const std::string& id) const override;
  std::string GetCohortHint(const std::string& id) const override;
  std::string GetCohortName(const std::string& id) const override;
  void SetCohort(const std::string& id, const std::string& cohort) override;
  void SetCohortHint(const std::string& id,
                     const std::string& cohort_hint) override;
  void SetCohortName(const std::string& id,
                     const std::string& cohort_name) override;
  void GetActiveBits(const std::vector<std::string>& ids,
                     base::OnceCallback<void(const std::set<std::string>&)>
                         callback) const override;
  int GetDaysSinceLastRollCall(const std::string& id) const override;
  int GetDaysSinceLastActive(const std::string& id) const override;
  base::Version GetProductVersion(const std::string& id) const override;
  void SetProductVersion(const std::string& id,
                         const base::Version& pv) override;
  base::Version GetMaxPreviousProductVersion(
      const std::string& id) const override;
  void SetMaxPreviousProductVersion(const std::string& id,
                                    const base::Version& max_version) override;

  std::string GetFingerprint(const std::string& id) const override;
  void SetFingerprint(const std::string& id,
                      const std::string& fingerprint) override;
  base::Time GetThrottleUpdatesUntil() const override;
  void SetThrottleUpdatesUntil(const base::Time& time) override;
  void SetLastUpdateCheckError(const CategorizedError& error) override;

 private:
  // Returns nullptr if the app key does not exist.
  const base::Value::Dict* GetAppKey(const std::string& id) const;

  // Returns an existing or newly created app key under a root pref.
  base::Value::Dict* GetOrCreateAppKey(const std::string& id,
                                       base::Value::Dict& root);

  // Returns fallback if the key does not exist.
  int GetInt(const std::string& id, const std::string& key, int fallback) const;

  // Returns the empty string if the key does not exist.
  std::string GetString(const std::string& id, const std::string& key) const;

  void SetString(const std::string& id,
                 const std::string& key,
                 const std::string& value);

  void SetDateLastDataHelper(const std::vector<std::string>& ids,
                             int datenum,
                             base::OnceClosure callback,
                             const std::set<std::string>& active_ids);

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ptr<PrefService, LeakedDanglingUntriaged> pref_service_;
  std::unique_ptr<ActivityDataService> activity_data_service_;
};

PersistedDataImpl::PersistedDataImpl(
    PrefService* pref_service,
    std::unique_ptr<ActivityDataService> activity_data_service)
    : pref_service_(pref_service),
      activity_data_service_(std::move(activity_data_service)) {
  CHECK(pref_service_->FindPreference(kPersistedDataPreference));
}

PersistedDataImpl::~PersistedDataImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const base::Value::Dict* PersistedDataImpl::GetAppKey(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return nullptr;
  }
  const base::Value& dict = pref_service_->GetValue(kPersistedDataPreference);
  if (!dict.is_dict()) {
    return nullptr;
  }
  const base::Value::Dict* apps = dict.GetDict().FindDict("apps");
  if (!apps) {
    return nullptr;
  }
  return apps->FindDict(base::ToLowerASCII(id));
}

int PersistedDataImpl::GetInt(const std::string& id,
                              const std::string& key,
                              int fallback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value::Dict* app_key = GetAppKey(id);
  if (!app_key) {
    return fallback;
  }
  return app_key->FindInt(key).value_or(fallback);
}

std::string PersistedDataImpl::GetString(const std::string& id,
                                         const std::string& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value::Dict* app_key = GetAppKey(id);
  if (!app_key) {
    return {};
  }
  const std::string* value = app_key->FindString(key);
  if (!value) {
    return {};
  }
  return *value;
}

int PersistedDataImpl::GetDateLastRollCall(const std::string& id) const {
  return GetInt(id, "dlrc", kDateUnknown);
}

int PersistedDataImpl::GetDateLastActive(const std::string& id) const {
  return GetInt(id, "dla", kDateUnknown);
}

std::string PersistedDataImpl::GetPingFreshness(const std::string& id) const {
  std::string result = GetString(id, "pf");
  return !result.empty() ? base::StringPrintf("{%s}", result.c_str()) : result;
}

int PersistedDataImpl::GetInstallDate(const std::string& id) const {
  return GetInt(id, "installdate", kDateUnknown);
}

std::string PersistedDataImpl::GetCohort(const std::string& id) const {
  return GetString(id, "cohort");
}

std::string PersistedDataImpl::GetCohortName(const std::string& id) const {
  return GetString(id, "cohortname");
}

std::string PersistedDataImpl::GetCohortHint(const std::string& id) const {
  return GetString(id, "cohorthint");
}

base::Value::Dict* PersistedDataImpl::GetOrCreateAppKey(
    const std::string& id,
    base::Value::Dict& root) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict* apps = root.EnsureDict("apps");
  base::Value::Dict* app = apps->FindDict(base::ToLowerASCII(id));
  if (!app) {
    app = &apps->Set(base::ToLowerASCII(id), base::Value::Dict())->GetDict();
    app->Set("installdate", kDateFirstTime);
  }
  return app;
}

void PersistedDataImpl::SetDateLastDataHelper(
    const std::vector<std::string>& ids,
    int datenum,
    base::OnceClosure callback,
    const std::set<std::string>& active_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
  for (const auto& id : ids) {
    base::Value::Dict* app_key = GetOrCreateAppKey(id, update.Get());
    app_key->Set("dlrc", datenum);
    app_key->Set("pf", base::Uuid::GenerateRandomV4().AsLowercaseString());
    if (GetInstallDate(id) == kDateFirstTime) {
      app_key->Set("installdate", datenum);
    }
    if (active_ids.find(id) != active_ids.end()) {
      app_key->Set("dla", datenum);
    }
  }
  std::move(callback).Run();
}

void PersistedDataImpl::SetDateLastData(const std::vector<std::string>& ids,
                                        int datenum,
                                        base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_ || datenum < 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }
  if (!activity_data_service_) {
    SetDateLastDataHelper(ids, datenum, std::move(callback), {});
    return;
  }
  activity_data_service_->GetAndClearActiveBits(
      ids, base::BindOnce(&PersistedDataImpl::SetDateLastDataHelper,
                          base::Unretained(this), ids, datenum,
                          std::move(callback)));
}

void PersistedDataImpl::SetDateLastActive(const std::string& id, int dla) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
  base::Value::Dict* app_key = GetOrCreateAppKey(id, update.Get());
  app_key->Set("dla", dla);
}

void PersistedDataImpl::SetDateLastRollCall(const std::string& id, int dlrc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
  base::Value::Dict* app_key = GetOrCreateAppKey(id, update.Get());
  app_key->Set("dlrc", dlrc);
}

void PersistedDataImpl::SetInstallDate(const std::string& id,
                                       int install_date) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
  base::Value::Dict* app_key = GetOrCreateAppKey(id, update.Get());
  app_key->Set("installdate", install_date);
}

void PersistedDataImpl::SetString(const std::string& id,
                                  const std::string& key,
                                  const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return;
  }
  ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
  GetOrCreateAppKey(id, update.Get())->Set(key, value);
}

void PersistedDataImpl::SetCohort(const std::string& id,
                                  const std::string& cohort) {
  SetString(id, "cohort", cohort);
}

void PersistedDataImpl::SetCohortName(const std::string& id,
                                      const std::string& cohort_name) {
  SetString(id, "cohortname", cohort_name);
}

void PersistedDataImpl::SetCohortHint(const std::string& id,
                                      const std::string& cohort_hint) {
  SetString(id, "cohorthint", cohort_hint);
}

void PersistedDataImpl::GetActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!activity_data_service_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::set<std::string>{}));
    return;
  }
  activity_data_service_->GetActiveBits(ids, std::move(callback));
}

int PersistedDataImpl::GetDaysSinceLastRollCall(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return activity_data_service_
             ? activity_data_service_->GetDaysSinceLastRollCall(id)
             : kDaysUnknown;
}

int PersistedDataImpl::GetDaysSinceLastActive(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return activity_data_service_
             ? activity_data_service_->GetDaysSinceLastActive(id)
             : kDaysUnknown;
}

base::Version PersistedDataImpl::GetProductVersion(
    const std::string& id) const {
  return base::Version(GetString(id, "pv"));
}

void PersistedDataImpl::SetProductVersion(const std::string& id,
                                          const base::Version& pv) {
  CHECK(pv.IsValid());
  SetString(id, "pv", pv.GetString());
}

base::Version PersistedDataImpl::GetMaxPreviousProductVersion(
    const std::string& id) const {
  return base::Version(GetString(id, "max_pv"));
}

void PersistedDataImpl::SetMaxPreviousProductVersion(
    const std::string& id,
    const base::Version& max_version) {
  CHECK(max_version.IsValid());
  auto existing_max = GetMaxPreviousProductVersion(id);
  if (!existing_max.IsValid() || max_version > existing_max) {
    SetString(id, "max_pv", max_version.GetString());
  }
}

std::string PersistedDataImpl::GetFingerprint(const std::string& id) const {
  return GetString(id, "fp");
}

void PersistedDataImpl::SetFingerprint(const std::string& id,
                                       const std::string& fingerprint) {
  SetString(id, "fp", fingerprint);
}

base::Time PersistedDataImpl::GetThrottleUpdatesUntil() const {
  return pref_service_->GetTime(kThrottleUpdatesUntilPreference);
}

void PersistedDataImpl::SetThrottleUpdatesUntil(const base::Time& time) {
  pref_service_->SetTime(kThrottleUpdatesUntilPreference, time);
}

void PersistedDataImpl::SetLastUpdateCheckError(const CategorizedError& error) {
  pref_service_->SetInteger(kLastUpdateCheckErrorPreference, error.code_);
  pref_service_->SetInteger(kLastUpdateCheckErrorCategoryPreference,
                            static_cast<int>(error.category_));
  pref_service_->SetInteger(kLastUpdateCheckErrorExtraCode1Preference,
                            error.extra_);
}

}  // namespace

std::unique_ptr<PersistedData> CreatePersistedData(
    PrefService* pref_service,
    std::unique_ptr<ActivityDataService> activity_data_service) {
  return std::make_unique<PersistedDataImpl>(pref_service,
                                             std::move(activity_data_service));
}

void RegisterPersistedDataPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPersistedDataPreference);
  registry->RegisterTimePref(kThrottleUpdatesUntilPreference, base::Time());
  registry->RegisterIntegerPref(kLastUpdateCheckErrorPreference, 0);
  registry->RegisterIntegerPref(kLastUpdateCheckErrorCategoryPreference, 0);
  registry->RegisterIntegerPref(kLastUpdateCheckErrorExtraCode1Preference, 0);
}

}  // namespace update_client
