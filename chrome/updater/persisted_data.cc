// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/persisted_data.h"

#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/registration_data.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_client_errors.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/registry.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace {

// PersistedData keys.
constexpr char kVersionPath[] = "pv_path";
constexpr char kVersionKey[] = "pv_key";
constexpr char kECP[] = "ecp";
constexpr char kBC[] = "bc";
constexpr char kBP[] = "bp";
constexpr char kAP[] = "ap";
constexpr char kAPPath[] = "ap_path";
constexpr char kAPKey[] = "ap_key";

constexpr char kHadApps[] = "had_apps";
constexpr char kUsageStatsEnabledKey[] = "usage_stats_enabled";
constexpr char kEulaRequired[] = "eula_required";

constexpr char kLastChecked[] = "last_checked";
constexpr char kLastStarted[] = "last_started";
constexpr char kLastOSVersion[] = "last_os_version";

}  // namespace

namespace updater {

PersistedData::PersistedData(
    UpdaterScope scope,
    PrefService* pref_service,
    std::unique_ptr<update_client::ActivityDataService> activity_service)
    : scope_(scope),
      pref_service_(pref_service),
      delegate_(
          update_client::CreatePersistedData(pref_service,
                                             std::move(activity_service))) {
  CHECK(pref_service_);
}

PersistedData::~PersistedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::Version PersistedData::GetProductVersion(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetProductVersion(id);
}

void PersistedData::SetProductVersion(const std::string& id,
                                      const base::Version& pv) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(pv.IsValid());
  delegate_->SetProductVersion(id, pv);

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, we record the PV in ClientState as well.
  // (Some applications read it from there.) This has the side effect of
  // creating the ClientState key, which is read to sense for application
  // uninstallation.
  SetRegistryKey(UpdaterScopeToHKeyRoot(scope_),
                 GetAppClientStateKey(base::UTF8ToWide(id)), kRegValuePV,
                 base::UTF8ToWide(pv.GetString()));
#endif
}

base::Version PersistedData::GetMaxPreviousProductVersion(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetMaxPreviousProductVersion(id);
}

void PersistedData::SetMaxPreviousProductVersion(
    const std::string& id,
    const base::Version& max_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(max_version.IsValid());
  delegate_->SetMaxPreviousProductVersion(id, max_version);
}

base::FilePath PersistedData::GetProductVersionPath(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FilePath::FromUTF8Unsafe(GetString(id, kVersionPath));
}

void PersistedData::SetProductVersionPath(const std::string& id,
                                          const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kVersionPath, path.AsUTF8Unsafe());
}

std::string PersistedData::GetProductVersionKey(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetString(id, kVersionKey);
}

void PersistedData::SetProductVersionKey(const std::string& id,
                                         const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kVersionKey, key);
}

std::string PersistedData::GetFingerprint(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetFingerprint(id);
}

void PersistedData::SetFingerprint(const std::string& id,
                                   const std::string& fingerprint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetFingerprint(id, fingerprint);
}

base::FilePath PersistedData::GetExistenceCheckerPath(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FilePath::FromUTF8Unsafe(GetString(id, kECP));
}

void PersistedData::SetExistenceCheckerPath(const std::string& id,
                                            const base::FilePath& ecp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kECP, ecp.AsUTF8Unsafe());
}

std::string PersistedData::GetBrandCode(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string bc = GetString(id, kBC);

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, if there is a brand code in the registry
  // ClientState, that brand code is considered authoritative, and overrides any
  // brand code that is already in `prefs`.
  std::wstring registry_bc;
  if (base::win::RegKey(UpdaterScopeToHKeyRoot(scope_),
                        GetAppClientStateKey(base::UTF8ToWide(id)).c_str(),
                        Wow6432(KEY_QUERY_VALUE))
          .ReadValue(kRegValueBrandCode, &registry_bc) == ERROR_SUCCESS) {
    const std::string registry_brand_code = base::WideToUTF8(registry_bc);
    if (!registry_brand_code.empty() && registry_brand_code != bc) {
      SetString(id, kBC, registry_brand_code);
      return registry_brand_code;
    }
  }
#endif

  if (bc.empty()) {
    return {};
  }

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, record the brand code in ClientState, since
  // some applications read it from there.
  SetRegistryKey(UpdaterScopeToHKeyRoot(scope_),
                 GetAppClientStateKey(base::UTF8ToWide(id)), kRegValueBrandCode,
                 base::UTF8ToWide(bc));
#endif
  return bc;
}

void PersistedData::SetBrandCode(const std::string& id, const std::string& bc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there is already an existing brand code, do not overwrite it.
  if (!GetBrandCode(id).empty()) {
    return;
  }

  SetString(id, kBC, bc);

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, record the brand code in ClientState, since
  // some applications read it from there.
  SetRegistryKey(UpdaterScopeToHKeyRoot(scope_),
                 GetAppClientStateKey(base::UTF8ToWide(id)), kRegValueBrandCode,
                 base::UTF8ToWide(bc));
#endif
}

base::FilePath PersistedData::GetBrandPath(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FilePath::FromUTF8Unsafe(GetString(id, kBP));
}

void PersistedData::SetBrandPath(const std::string& id,
                                 const base::FilePath& bp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kBP, bp.AsUTF8Unsafe());
}

std::string PersistedData::GetAP(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, we read AP from ClientState first, since some
  // applications write to it there.
  if (const std::string ap(GetAppAPValue(scope_, id)); !ap.empty()) {
    SetAP(id, ap);
    return ap;
  }
#endif

  return GetString(id, kAP);
}

void PersistedData::SetAP(const std::string& id, const std::string& ap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kAP, ap);

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, we record the AP in ClientState as well.
  // (Some applications read it from there.)

  // Chromium Updater has both local and global pref stores. In practice, if
  // this `PersistedData` is using a local pref store, `id` will be the
  // qualification app and the ClientState value is not important, so it is
  // acceptable for each instance of the updater to overwrite it with various
  // values. Else, this is the global pref store and reflecting the value in
  // registry is correct. Clients should transition to requesting the
  // registration info for their application via RPC.
  SetRegistryKey(UpdaterScopeToHKeyRoot(scope_),
                 GetAppClientStateKey(base::UTF8ToWide(id)), kRegValueAP,
                 base::UTF8ToWide(ap));
#endif
}

base::FilePath PersistedData::GetAPPath(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FilePath::FromUTF8Unsafe(GetString(id, kAPPath));
}

void PersistedData::SetAPPath(const std::string& id,
                              const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kAPPath, path.AsUTF8Unsafe());
}

std::string PersistedData::GetAPKey(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetString(id, kAPKey);
}

void PersistedData::SetAPKey(const std::string& id, const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kAPKey, key);
}

int PersistedData::GetDateLastActive(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetDateLastActive(id);
}

int PersistedData::GetDaysSinceLastActive(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetDaysSinceLastActive(id);
}

void PersistedData::SetDateLastActive(const std::string& id, int dla) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetDateLastActive(id, dla);
}

int PersistedData::GetDateLastRollCall(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetDateLastRollCall(id);
}

int PersistedData::GetDaysSinceLastRollCall(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetDaysSinceLastRollCall(id);
}

void PersistedData::SetDateLastRollCall(const std::string& id, int dlrc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetDateLastRollCall(id, dlrc);
}

std::string PersistedData::GetCohort(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetCohort(id);
}

void PersistedData::SetCohort(const std::string& id,
                              const std::string& cohort) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetCohort(id, cohort);

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, we record the Cohort in ClientState as well.
  // (Some applications read it from there.)
  SetRegistryKey(UpdaterScopeToHKeyRoot(scope_),
                 GetAppCohortKey(base::UTF8ToWide(id)), L"",
                 base::UTF8ToWide(cohort));
#endif
}

std::string PersistedData::GetCohortName(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetCohortName(id);
}

void PersistedData::SetCohortName(const std::string& id,
                                  const std::string& cohort_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetCohortName(id, cohort_name);

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, we record the Cohort in ClientState as well.
  // (Some applications read it from there.)
  SetRegistryKey(UpdaterScopeToHKeyRoot(scope_),
                 GetAppCohortKey(base::UTF8ToWide(id)), kRegValueCohortName,
                 base::UTF8ToWide(cohort_name));
#endif
}

std::string PersistedData::GetCohortHint(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetCohortHint(id);
}

void PersistedData::SetCohortHint(const std::string& id,
                                  const std::string& cohort_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetCohortHint(id, cohort_hint);
}

std::string PersistedData::GetPingFreshness(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetPingFreshness(id);
}

void PersistedData::SetDateLastData(const std::vector<std::string>& ids,
                                    int datenum,
                                    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetDateLastData(ids, datenum, std::move(callback));
}

int PersistedData::GetInstallDate(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetInstallDate(id);
}

void PersistedData::SetInstallDate(const std::string& id, int install_date) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetInstallDate(id, install_date);
}

void PersistedData::GetActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->GetActiveBits(ids, std::move(callback));
}

base::Time PersistedData::GetThrottleUpdatesUntil() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetThrottleUpdatesUntil();
}

void PersistedData::SetLastUpdateCheckError(
    const update_client::CategorizedError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetLastUpdateCheckError(error);
}

void PersistedData::SetThrottleUpdatesUntil(const base::Time& time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetThrottleUpdatesUntil(time);
}

void PersistedData::RegisterApp(const RegistrationRequest& rq) {
  VLOG(2) << __func__ << ": Registering " << rq.app_id;
  if (rq.version.IsValid()) {
    VLOG(2) << __func__ << ": app version " << rq.version;
    SetProductVersion(rq.app_id, rq.version);
  }
  if (!rq.version_path.empty()) {
    SetProductVersionPath(rq.app_id, rq.version_path);
  }
  if (!rq.version_key.empty()) {
    SetProductVersionKey(rq.app_id, rq.version_key);
  }
  if (!rq.existence_checker_path.empty()) {
    SetExistenceCheckerPath(rq.app_id, rq.existence_checker_path);
  }
  if (!rq.brand_code.empty()) {
    SetBrandCode(rq.app_id, rq.brand_code);
  }
  if (!rq.brand_path.empty()) {
    SetBrandPath(rq.app_id, rq.brand_path);
  }
  if (!rq.ap.empty()) {
    SetAP(rq.app_id, rq.ap);
  }
  if (!rq.ap_path.empty()) {
    SetAPPath(rq.app_id, rq.ap_path);
  }
  if (!rq.ap_key.empty()) {
    SetAPKey(rq.app_id, rq.ap_key);
  }
  if (rq.dla) {
    SetDateLastActive(rq.app_id, rq.dla.value());
  } else if (GetDateLastActive(rq.app_id) == update_client::kDateUnknown) {
    SetDateLastActive(rq.app_id, update_client::kDateFirstTime);
  }
  if (rq.dlrc) {
    SetDateLastRollCall(rq.app_id, rq.dlrc.value());
  } else if (GetDateLastRollCall(rq.app_id) == update_client::kDateUnknown) {
    SetDateLastRollCall(rq.app_id, update_client::kDateFirstTime);
  }
  if (rq.install_date) {
    SetInstallDate(rq.app_id, *rq.install_date);
  }
  if (!rq.cohort.empty()) {
    SetCohort(rq.app_id, rq.cohort);
  }
  if (!rq.cohort_name.empty()) {
    SetCohortName(rq.app_id, rq.cohort_name);
  }
  if (!rq.cohort_hint.empty()) {
    SetCohortHint(rq.app_id, rq.cohort_hint);
  }
}

bool PersistedData::RemoveApp(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return false;
  }

  ScopedDictPrefUpdate update(pref_service_,
                              update_client::kPersistedDataPreference);
  base::Value::Dict* apps = update->FindDict("apps");

  return apps ? apps->Remove(base::ToLowerASCII(id)) : false;
}

std::vector<std::string> PersistedData::GetAppIds() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The prefs is a dictionary of dictionaries, where each inner dictionary
  // corresponds to an app:
  // {"updateclientdata":{"apps":{"{44FC7FE2-65CE-487C-93F4-EDEE46EEAAAB}":{...
  const base::Value::Dict& dict =
      pref_service_->GetDict(update_client::kPersistedDataPreference);
  const base::Value::Dict* apps = dict.FindDict("apps");
  if (!apps) {
    return {};
  }
  std::vector<std::string> app_ids;
  for (auto it = apps->begin(); it != apps->end(); ++it) {
    const auto& app_id = it->first;
    const auto pv = GetProductVersion(app_id);
    if (pv.IsValid()) {
      app_ids.push_back(app_id);
    }
  }
  return app_ids;
}

const base::Value::Dict* PersistedData::GetAppKey(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return nullptr;
  }
  const base::Value::Dict& dict =
      pref_service_->GetDict(update_client::kPersistedDataPreference);
  const base::Value::Dict* apps = dict.FindDict("apps");
  if (!apps) {
    return nullptr;
  }
  return apps->FindDict(base::ToLowerASCII(id));
}

std::string PersistedData::GetString(const std::string& id,
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

base::Value::Dict* PersistedData::GetOrCreateAppKey(const std::string& id,
                                                    base::Value::Dict& root) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict* apps = root.EnsureDict("apps");
  base::Value::Dict* app = apps->EnsureDict(base::ToLowerASCII(id));
  return app;
}

std::optional<int> PersistedData::GetInteger(const std::string& id,
                                             const std::string& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return std::nullopt;
  }
  ScopedDictPrefUpdate update(pref_service_,
                              update_client::kPersistedDataPreference);
  base::Value::Dict* apps = update->FindDict("apps");
  if (!apps) {
    return std::nullopt;
  }
  base::Value::Dict* app = apps->FindDict(base::ToLowerASCII(id));
  if (!app) {
    return std::nullopt;
  }
  return app->FindInt(key);
}

void PersistedData::SetInteger(const std::string& id,
                               const std::string& key,
                               int value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return;
  }
  ScopedDictPrefUpdate update(pref_service_,
                              update_client::kPersistedDataPreference);
  GetOrCreateAppKey(id, update.Get())->Set(key, value);
}

void PersistedData::SetString(const std::string& id,
                              const std::string& key,
                              const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return;
  }
  ScopedDictPrefUpdate update(pref_service_,
                              update_client::kPersistedDataPreference);
  GetOrCreateAppKey(id, update.Get())->Set(key, value);
}

bool PersistedData::GetHadApps() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_ && pref_service_->GetBoolean(kHadApps);
}

void PersistedData::SetHadApps() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetBoolean(kHadApps, true);
  }
}

bool PersistedData::GetUsageStatsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_ && pref_service_->GetBoolean(kUsageStatsEnabledKey);
}

void PersistedData::SetUsageStatsEnabled(bool usage_stats_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetBoolean(kUsageStatsEnabledKey, usage_stats_enabled);
  }
}

bool PersistedData::GetEulaRequired() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_ && pref_service_->GetBoolean(kEulaRequired);
}

void PersistedData::SetEulaRequired(bool eula_required) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetBoolean(kEulaRequired, eula_required);
  }
#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, `eulaaccepted` is recorded in the registry,
  // since some applications read it from there.
  SetEulaAccepted(scope_, !eula_required);
#endif
}

base::Time PersistedData::GetLastChecked() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetTime(kLastChecked);
}

void PersistedData::SetLastChecked(const base::Time& time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetTime(kLastChecked, time);
  }
}

base::Time PersistedData::GetLastStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetTime(kLastStarted);
}

void PersistedData::SetLastStarted(const base::Time& time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetTime(kLastStarted, time);
  }
}

#if BUILDFLAG(IS_WIN)
std::optional<OSVERSIONINFOEX> PersistedData::GetLastOSVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Unpacks the os version from a base-64-encoded string internally.
  const std::string encoded_os_version =
      pref_service_->GetString(kLastOSVersion);

  if (encoded_os_version.empty()) {
    return std::nullopt;
  }

  const std::optional<std::vector<uint8_t>> decoded_os_version =
      base::Base64Decode(encoded_os_version);
  if (!decoded_os_version ||
      decoded_os_version->size() != sizeof(OSVERSIONINFOEX)) {
    return std::nullopt;
  }

  return *reinterpret_cast<const OSVERSIONINFOEX*>(decoded_os_version->data());
}

void PersistedData::SetLastOSVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pref_service_) {
    return;
  }

  // Get and set the current OS version.
  std::optional<OSVERSIONINFOEX> os_version = GetOSVersion();
  if (!os_version) {
    return;
  }

  // The os version is internally stored as a base-64-encoded string.
  std::string encoded_os_version =
      base::Base64Encode(base::byte_span_from_ref(os_version.value()));

  return pref_service_->SetString(kLastOSVersion, encoded_os_version);
}
#endif

// Register persisted data prefs, except for kPersistedDataPreference.
// kPersistedDataPreference is registered by update_client::RegisterPrefs.
void RegisterPersistedDataPrefs(scoped_refptr<PrefRegistrySimple> registry) {
  registry->RegisterBooleanPref(kHadApps, false);
  registry->RegisterBooleanPref(kUsageStatsEnabledKey, false);
  registry->RegisterBooleanPref(kEulaRequired, false);
  registry->RegisterTimePref(kLastChecked, {});
  registry->RegisterTimePref(kLastStarted, {});
  registry->RegisterStringPref(kLastOSVersion, {});
}

}  // namespace updater
