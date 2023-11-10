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
#include "base/strings/string_piece.h"
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
#include "components/update_client/persisted_data.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace {

// PersistedData keys.
constexpr char kPV[] = "pv";
constexpr char kVersionPath[] = "pv_path";
constexpr char kVersionKey[] = "pv_key";
constexpr char kFP[] = "fp";
constexpr char kECP[] = "ecp";
constexpr char kBC[] = "bc";
constexpr char kBP[] = "bp";
constexpr char kAP[] = "ap";
constexpr char kAPPath[] = "ap_path";
constexpr char kAPKey[] = "ap_key";
constexpr char kDLA[] = "dla";
constexpr char kDLRC[] = "dlrc";
constexpr char kCohort[] = "cohort";
constexpr char kCohortName[] = "cohortname";
constexpr char kCohortHint[] = "cohorthint";

constexpr char kHadApps[] = "had_apps";
constexpr char kUsageStatsEnabledKey[] = "usage_stats_enabled";

constexpr char kLastChecked[] = "last_checked";
constexpr char kLastStarted[] = "last_started";
constexpr char kLastOSVersion[] = "last_os_version";

}  // namespace

namespace updater {

PersistedData::PersistedData(UpdaterScope scope, PrefService* pref_service)
    : scope_(scope), pref_service_(pref_service) {
  CHECK(pref_service_);
  CHECK(pref_service_->FindPreference(update_client::kPersistedDataPreference));
}

PersistedData::~PersistedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::Version PersistedData::GetProductVersion(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Version(GetString(id, kPV));
}

void PersistedData::SetProductVersion(const std::string& id,
                                      const base::Version& pv) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(pv.IsValid());
  SetString(id, kPV, pv.GetString());
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
  return GetString(id, kFP);
}

void PersistedData::SetFingerprint(const std::string& id,
                                   const std::string& fingerprint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kFP, fingerprint);
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

std::string PersistedData::GetBrandCode(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetString(id, kBC);
}

void PersistedData::SetBrandCode(const std::string& id, const std::string& bc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kBC, bc);
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
                 GetAppClientStateKey(base::SysUTF8ToWide(id)), L"ap",
                 base::SysUTF8ToWide(ap));
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

std::optional<int> PersistedData::GetDateLastActive(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetInteger(id, kDLA);
}

void PersistedData::SetDateLastActive(const std::string& id, int dla) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetInteger(id, kDLA, dla);
}

std::optional<int> PersistedData::GetDateLastRollcall(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetInteger(id, kDLRC);
}

void PersistedData::SetDateLastRollcall(const std::string& id, int dlrc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetInteger(id, kDLRC, dlrc);
}

std::string PersistedData::GetCohort(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetString(id, kCohort);
}

void PersistedData::SetCohort(const std::string& id,
                              const std::string& cohort) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kCohort, cohort);

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, we record the Cohort in ClientState as well.
  // (Some applications read it from there.)
  SetRegistryKey(UpdaterScopeToHKeyRoot(scope_),
                 GetAppCohortKey(base::SysUTF8ToWide(id)), L"",
                 base::SysUTF8ToWide(cohort));
#endif
}

std::string PersistedData::GetCohortName(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetString(id, kCohortName);
}

void PersistedData::SetCohortName(const std::string& id,
                                  const std::string& cohort_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kCohortName, cohort_name);

#if BUILDFLAG(IS_WIN)
  // For backwards compatibility, we record the Cohort in ClientState as well.
  // (Some applications read it from there.)
  SetRegistryKey(UpdaterScopeToHKeyRoot(scope_),
                 GetAppCohortKey(base::SysUTF8ToWide(id)), kRegValueCohortName,
                 base::SysUTF8ToWide(cohort_name));
#endif
}

std::string PersistedData::GetCohortHint(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetString(id, kCohortHint);
}

void PersistedData::SetCohortHint(const std::string& id,
                                  const std::string& cohort_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kCohortHint, cohort_hint);
}

void PersistedData::RegisterApp(const RegistrationRequest& rq) {
  VLOG(2) << __func__ << ": Registering " << rq.app_id << " at version "
          << rq.version;
  if (rq.version.IsValid()) {
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
  }
  if (rq.dlrc) {
    SetDateLastRollcall(rq.app_id, rq.dlrc.value());
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
  if (!pref_service_)
    return false;

  ScopedDictPrefUpdate update(pref_service_,
                              update_client::kPersistedDataPreference);
  base::Value::Dict* apps = update->FindDict("apps");

  return apps ? apps->Remove(id) : false;
}

std::vector<std::string> PersistedData::GetAppIds() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The prefs is a dictionary of dictionaries, where each inner dictionary
  // corresponds to an app:
  // {"updateclientdata":{"apps":{"{44FC7FE2-65CE-487C-93F4-EDEE46EEAAAB}":{...
  const base::Value::Dict& dict =
      pref_service_->GetDict(update_client::kPersistedDataPreference);
  const base::Value::Dict* apps = dict.FindDict("apps");
  if (!apps)
    return {};
  std::vector<std::string> app_ids;
  for (auto it = apps->begin(); it != apps->end(); ++it) {
    const auto& app_id = it->first;
    const auto pv = GetProductVersion(app_id);
    if (pv.IsValid())
      app_ids.push_back(app_id);
  }
  return app_ids;
}

const base::Value::Dict* PersistedData::GetAppKey(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_)
    return nullptr;
  const base::Value::Dict& dict =
      pref_service_->GetDict(update_client::kPersistedDataPreference);
  const base::Value::Dict* apps = dict.FindDict("apps");
  if (!apps)
    return nullptr;
  return apps->FindDict(base::ToLowerASCII(id));
}

std::string PersistedData::GetString(const std::string& id,
                                     const std::string& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value::Dict* app_key = GetAppKey(id);
  if (!app_key)
    return {};
  const std::string* value = app_key->FindString(key);
  if (!value)
    return {};
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
  if (!pref_service_)
    return;
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
  if (pref_service_)
    pref_service_->SetBoolean(kHadApps, true);
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

base::Time PersistedData::GetLastChecked() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetTime(kLastChecked);
}

void PersistedData::SetLastChecked(const base::Time& time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_)
    pref_service_->SetTime(kLastChecked, time);
}

base::Time PersistedData::GetLastStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetTime(kLastStarted);
}

void PersistedData::SetLastStarted(const base::Time& time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_)
    pref_service_->SetTime(kLastStarted, time);
}

#if BUILDFLAG(IS_WIN)
std::optional<OSVERSIONINFOEX> PersistedData::GetLastOSVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Unpacks the os version from a base-64-encoded string internally.
  const std::string encoded_os_version =
      pref_service_->GetString(kLastOSVersion);

  if (encoded_os_version.empty())
    return std::nullopt;

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

  if (!pref_service_)
    return;

  // Get and set the current OS version.
  std::optional<OSVERSIONINFOEX> os_version = GetOSVersion();
  if (!os_version)
    return;

  // The os version is internally stored as a base-64-encoded string.
  std::string encoded_os_version;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&os_version.value()),
                        sizeof(OSVERSIONINFOEX)),
      &encoded_os_version);
  return pref_service_->SetString(kLastOSVersion, encoded_os_version);
}
#endif

// Register persisted data prefs, except for kPersistedDataPreference.
// kPersistedDataPreference is registered by update_client::RegisterPrefs.
void RegisterPersistedDataPrefs(scoped_refptr<PrefRegistrySimple> registry) {
  registry->RegisterBooleanPref(kHadApps, false);
  registry->RegisterBooleanPref(kUsageStatsEnabledKey, false);
  registry->RegisterTimePref(kLastChecked, {});
  registry->RegisterTimePref(kLastStarted, {});
  registry->RegisterStringPref(kLastOSVersion, {});
}

}  // namespace updater
