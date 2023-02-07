// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs_impl.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/update_client/update_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

const char kPrefQualified[] = "qualified";
const char kPrefSwapping[] = "swapping";
const char kPrefMigratedLegacyUpdaters[] = "converted_legacy_updaters";
const char kPrefActiveVersion[] = "active_version";
const char kPrefServerStarts[] = "server_starts";

}  // namespace

UpdaterPrefsImpl::UpdaterPrefsImpl(std::unique_ptr<ScopedPrefsLock> lock,
                                   std::unique_ptr<PrefService> prefs)
    : lock_(std::move(lock)), prefs_(std::move(prefs)) {}

UpdaterPrefsImpl::~UpdaterPrefsImpl() = default;

PrefService* UpdaterPrefsImpl::GetPrefService() const {
  return prefs_.get();
}

bool UpdaterPrefsImpl::GetQualified() const {
  return prefs_->GetBoolean(kPrefQualified);
}

void UpdaterPrefsImpl::SetQualified(bool value) {
  prefs_->SetBoolean(kPrefQualified, value);
}

std::string UpdaterPrefsImpl::GetActiveVersion() const {
  return prefs_->GetString(kPrefActiveVersion);
}

void UpdaterPrefsImpl::SetActiveVersion(const std::string& value) {
  prefs_->SetString(kPrefActiveVersion, value);
}

bool UpdaterPrefsImpl::GetSwapping() const {
  return prefs_->GetBoolean(kPrefSwapping);
}

void UpdaterPrefsImpl::SetSwapping(bool value) {
  prefs_->SetBoolean(kPrefSwapping, value);
}

bool UpdaterPrefsImpl::GetMigratedLegacyUpdaters() const {
  return prefs_->GetBoolean(kPrefMigratedLegacyUpdaters);
}

void UpdaterPrefsImpl::SetMigratedLegacyUpdaters() {
  prefs_->SetBoolean(kPrefMigratedLegacyUpdaters, true);
}

int UpdaterPrefsImpl::CountServerStarts() {
  int starts = prefs_->GetInteger(kPrefServerStarts);
  if (starts <= kMaxServerStartsBeforeFirstReg)
    prefs_->SetInteger(kPrefServerStarts, ++starts);
  return starts;
}

scoped_refptr<GlobalPrefs> CreateGlobalPrefs(UpdaterScope scope) {
  std::unique_ptr<ScopedPrefsLock> lock =
      AcquireGlobalPrefsLock(scope, base::Minutes(2));
  if (!lock)
    return nullptr;

  const absl::optional<base::FilePath> global_prefs_dir =
      GetInstallDirectory(scope);
  if (!global_prefs_dir || !base::CreateDirectory(*global_prefs_dir)) {
    return nullptr;
  }
  VLOG(1) << "global_prefs_dir: " << global_prefs_dir;

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      global_prefs_dir->Append(FILE_PATH_LITERAL("prefs.json"))));

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());
  pref_registry->RegisterBooleanPref(kPrefSwapping, false);
  pref_registry->RegisterBooleanPref(kPrefMigratedLegacyUpdaters, false);
  pref_registry->RegisterStringPref(kPrefActiveVersion, "0");
  pref_registry->RegisterIntegerPref(kPrefServerStarts, 0);
  RegisterPersistedDataPrefs(pref_registry);

  return base::MakeRefCounted<UpdaterPrefsImpl>(
      std::move(lock), pref_service_factory.Create(pref_registry));
}

scoped_refptr<LocalPrefs> CreateLocalPrefs(UpdaterScope scope) {
  const absl::optional<base::FilePath> local_prefs_dir =
      GetVersionedInstallDirectory(scope);
  if (!local_prefs_dir || !base::CreateDirectory(*local_prefs_dir)) {
    return nullptr;
  }

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      local_prefs_dir->Append(FILE_PATH_LITERAL("prefs.json"))));

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());
  pref_registry->RegisterBooleanPref(kPrefQualified, false);
  RegisterPersistedDataPrefs(pref_registry);

  return base::MakeRefCounted<UpdaterPrefsImpl>(
      nullptr, pref_service_factory.Create(pref_registry));
}

void PrefsCommitPendingWrites(PrefService* pref_service) {
  base::WaitableEvent write_complete_event;
  pref_service->CommitPendingWrite({}, base::BindOnce(
                                           [](base::WaitableEvent& event) {
                                             VLOG(1) << "Prefs committed.";
                                             event.Signal();
                                           },
                                           std::ref(write_complete_event)));
  write_complete_event.Wait();
}

}  // namespace updater
