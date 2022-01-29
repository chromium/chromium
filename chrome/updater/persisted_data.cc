// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/persisted_data.h"

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/registration_data.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// Uses the same pref as the update_client code.
constexpr char kPersistedDataPreference[] = "updateclientdata";

constexpr char kPV[] = "pv";    // Key for storing product version.
constexpr char kFP[] = "fp";    // Key for storing fingerprint.
constexpr char kECP[] = "ecp";  // Key for storing existence checker path.
constexpr char kBC[] = "bc";    // Key for storing brand code.
constexpr char kBP[] = "bp";    // Key for storing brand path.
constexpr char kAP[] = "ap";    // Key for storing ap.

constexpr char kHadApps[] = "had_apps";

// TODO(crbug.com/1292189): rename "updater_time" to "last_checked".
constexpr char kLastChecked[] = "update_time";
constexpr char kLastStarted[] = "last_started";

}  // namespace

namespace updater {

PersistedData::PersistedData(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  DCHECK(pref_service_->FindPreference(kPersistedDataPreference));
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
}

void PersistedData::RegisterApp(const RegistrationRequest& rq) {
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

  DictionaryPrefUpdate update(pref_service_, kPersistedDataPreference);
  base::Value* apps = update->FindDictKey("apps");

  return apps ? apps->RemoveKey(id) : false;
}

std::vector<std::string> PersistedData::GetAppIds() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The prefs is a dictionary of dictionaries, where each inner dictionary
  // corresponds to an app:
  // {"updateclientdata":{"apps":{"{44FC7FE2-65CE-487C-93F4-EDEE46EEAAAB}":{...
  const auto* pref = pref_service_->GetDictionary(kPersistedDataPreference);
  if (!pref)
    return {};
  const auto* apps = pref->FindKey("apps");
  if (!apps || !apps->is_dict())
    return {};
  std::vector<std::string> app_ids;
  for (auto kv : apps->DictItems()) {
    const auto& app_id = kv.first;
    const auto pv = GetProductVersion(app_id);
    if (pv.IsValid())
      app_ids.push_back(app_id);
  }
  return app_ids;
}

const base::Value* PersistedData::GetAppKey(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_)
    return nullptr;
  const base::Value* dict =
      pref_service_->GetDictionary(kPersistedDataPreference);
  if (!dict)
    return nullptr;
  const base::Value* apps = dict->FindDictKey("apps");
  if (!apps)
    return nullptr;
  return apps->FindDictKey(id);
}

std::string PersistedData::GetString(const std::string& id,
                                     const std::string& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::Value* app_key = GetAppKey(id);
  if (!app_key)
    return {};
  const std::string* value = app_key->FindStringKey(key);
  if (!value)
    return {};
  return *value;
}

base::Value* PersistedData::GetOrCreateAppKey(const std::string& id,
                                              base::Value* root) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value* apps = root->FindDictKey("apps");
  if (!apps)
    apps = root->SetKey("apps", base::Value(base::Value::Type::DICTIONARY));
  base::Value* app = apps->FindDictKey(id);
  if (!app)
    app = apps->SetKey(id, base::Value(base::Value::Type::DICTIONARY));
  return app;
}

void PersistedData::SetString(const std::string& id,
                              const std::string& key,
                              const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_)
    return;
  DictionaryPrefUpdate update(pref_service_, kPersistedDataPreference);
  GetOrCreateAppKey(id, update.Get())->SetStringKey(key, value);
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

// Register persisted data prefs, except for kPersistedDataPreference.
// kPersistedDataPreference is registered by update_client::RegisterPrefs.
void RegisterPersistedDataPrefs(scoped_refptr<PrefRegistrySimple> registry) {
  registry->RegisterBooleanPref(kHadApps, false);
  registry->RegisterTimePref(kLastChecked, {});
  registry->RegisterTimePref(kLastStarted, {});
}

}  // namespace updater
