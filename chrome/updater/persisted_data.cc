// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/persisted_data.h"

#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/sys_string_conversions.h"
#include "chrome/updater/util/win_util.h"
#endif

namespace {

constexpr char kPV[] = "pv";    // Key for storing product version.
constexpr char kFP[] = "fp";    // Key for storing fingerprint.
constexpr char kECP[] = "ecp";  // Key for storing existence checker path.
constexpr char kBC[] = "bc";    // Key for storing brand code.
constexpr char kBP[] = "bp";    // Key for storing brand path.
constexpr char kAP[] = "ap";    // Key for storing ap.

constexpr char kHadApps[] = "had_apps";

constexpr char kLastChecked[] = "last_checked";
constexpr char kLastStarted[] = "last_started";
constexpr char kLastOSVersion[] = "last_os_version";

}  // namespace

namespace updater {

PersistedData::PersistedData(UpdaterScope scope, PrefService* pref_service)
    : scope_(scope), pref_service_(pref_service) {
  DCHECK(pref_service_);
  DCHECK(
      pref_service_->FindPreference(update_client::kPersistedDataPreference));
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
  DCHECK(pv.IsValid());
  SetString(id, kPV, pv.GetString());
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

std::string PersistedData::GetAP(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void PersistedData::RegisterApp(const RegistrationRequest& rq) {
  VLOG(2) << __func__ << ": Registering " << rq.app_id << " at version "
          << rq.version;
  SetProductVersion(rq.app_id, rq.version);
  SetExistenceCheckerPath(rq.app_id, rq.existence_checker_path);
  SetBrandCode(rq.app_id, rq.brand_code);
  SetBrandPath(rq.app_id, rq.brand_path);
  SetAP(rq.app_id, rq.ap);
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
  return apps->FindDict(id);
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
  base::Value::Dict* app = apps->EnsureDict(id);
  return app;
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
absl::optional<OSVERSIONINFOEX> PersistedData::GetLastOSVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Unpacks the os version from a base-64-encoded string internally.
  const std::string encoded_os_version =
      pref_service_->GetString(kLastOSVersion);

  if (encoded_os_version.empty())
    return absl::nullopt;

  const absl::optional<std::vector<uint8_t>> decoded_os_version =
      base::Base64Decode(encoded_os_version);
  if (!decoded_os_version ||
      decoded_os_version->size() != sizeof(OSVERSIONINFOEX)) {
    return absl::nullopt;
  }

  return *reinterpret_cast<const OSVERSIONINFOEX*>(decoded_os_version->data());
}

void PersistedData::SetLastOSVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pref_service_)
    return;

  // Get and set the current OS version.
  absl::optional<OSVERSIONINFOEX> os_version = GetOSVersion();
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
  registry->RegisterTimePref(kLastChecked, {});
  registry->RegisterTimePref(kLastStarted, {});
  registry->RegisterStringPref(kLastOSVersion, {});
}

}  // namespace updater
