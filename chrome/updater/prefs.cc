// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/prefs.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/lock.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs_impl.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/update_client/update_client.h"

namespace updater {

namespace {

const char kPrefQualified[] = "qualified";
const char kPrefSwapping[] = "swapping";
const char kPrefMigratedLegacyUpdaters[] = "converted_legacy_updaters";
const char kPrefActiveVersion[] = "active_version";
const char kPrefServerStarts[] = "server_starts";

// Serializes access to prefs.
const char kPrefsAccessMutex[] = PREFS_ACCESS_MUTEX;

// Total time to wait when creating prefs.
constexpr base::TimeDelta kCreatePrefsWait(base::Minutes(2));

// The prefs can fail to load, for example with `PREF_READ_ERROR_FILE_LOCKED`,
// so this function retries a few times.
std::unique_ptr<PrefService> CreatePrefService(
    const base::FilePath& prefs_dir,
    scoped_refptr<PrefRegistrySimple> pref_registry,
    base::TimeDelta wait_period) {
  const auto deadline(base::TimeTicks::Now() + wait_period);
  do {
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
        prefs_dir.Append(FILE_PATH_LITERAL("prefs.json"))));

    std::unique_ptr<PrefService> pref_service(
        pref_service_factory.Create(pref_registry));
    if (!pref_service) {
      return nullptr;
    }

    if ((pref_service->GetInitializationStatus() ==
         PrefService::INITIALIZATION_STATUS_SUCCESS) ||
        (pref_service->GetInitializationStatus() ==
         PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE)) {
      return pref_service;
    }

    VLOG(1) << "pref service init failed: "
            << pref_service->GetInitializationStatus();

    base::PlatformThread::Sleep(base::Milliseconds(10));
  } while (base::TimeTicks::Now() < deadline);
  return nullptr;
}

scoped_refptr<GlobalPrefs> CreateGlobalPrefsInternal(
    UpdaterScope scope,
    base::FunctionRef<bool(UpdaterScope)> check_wrong_user = &WrongUser) {
  VLOG(2) << __func__;
  if (check_wrong_user(scope)) {
    VLOG(0) << "Current user is incompatible with scope " << scope
            << "; GlobalPrefs will not be created.";
    return nullptr;
  }

  const auto deadline(base::TimeTicks::Now() + kCreatePrefsWait);
  std::unique_ptr<ScopedLock> lock =
      CreateScopedLock(kPrefsAccessMutex, scope, kCreatePrefsWait);
  if (!lock) {
    LOG(ERROR) << "Failed to acquire GlobalPrefs";
    return nullptr;
  }

  const std::optional<base::FilePath> global_prefs_dir =
      GetInstallDirectory(scope);
  if (!global_prefs_dir || !base::CreateDirectory(*global_prefs_dir)) {
    return nullptr;
  }

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());
  pref_registry->RegisterBooleanPref(kPrefSwapping, false);
  pref_registry->RegisterBooleanPref(kPrefMigratedLegacyUpdaters, false);
  pref_registry->RegisterStringPref(kPrefActiveVersion, "0");
  pref_registry->RegisterIntegerPref(kPrefServerStarts, 0);
  RegisterPersistedDataPrefs(pref_registry);

  std::unique_ptr<PrefService> pref_service(CreatePrefService(
      *global_prefs_dir, pref_registry,
      std::max(deadline - base::TimeTicks::Now(), base::Seconds(0))));
  if (!pref_service) {
    return nullptr;
  }

  return base::MakeRefCounted<UpdaterPrefsImpl>(
      *global_prefs_dir, std::move(lock), std::move(pref_service));
}

}  // namespace

UpdaterPrefsImpl::UpdaterPrefsImpl(const base::FilePath& prefs_dir,
                                   std::unique_ptr<ScopedLock> lock,
                                   std::unique_ptr<PrefService> prefs)
    : prefs_dir_(prefs_dir), lock_(std::move(lock)), prefs_(std::move(prefs)) {
  VLOG(1) << __func__ << (lock_.get() ? " (global): " : " (local): ")
          << prefs_dir_;
}

UpdaterPrefsImpl::~UpdaterPrefsImpl() {
  VLOG(1) << __func__ << ": " << prefs_dir_;
}

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
  if (starts <= kMaxServerStartsBeforeFirstReg) {
    prefs_->SetInteger(kPrefServerStarts, ++starts);
  }
  return starts;
}

scoped_refptr<GlobalPrefs> CreateGlobalPrefs(UpdaterScope scope) {
  return CreateGlobalPrefsInternal(scope, &WrongUser);
}

// Overrides `check_wrong_user` to always return `false` when calling
// `CreateGlobalPrefsInternal`. This allows the test driver to allow creating
// the global prefs even if running at high integrity, such as in the
// `IntegrationTestUserInSystem.ElevatedInstallOfUserUpdaterAndApp` test.
scoped_refptr<GlobalPrefs> CreateGlobalPrefsForTesting(UpdaterScope scope) {
  return CreateGlobalPrefsInternal(
      scope, /*check_wrong_user=*/[](UpdaterScope /*scope*/) { return false; });
}

scoped_refptr<LocalPrefs> CreateLocalPrefs(UpdaterScope scope) {
  VLOG(2) << __func__;
  const std::optional<base::FilePath> local_prefs_dir =
      GetVersionedInstallDirectory(scope);
  if (!local_prefs_dir || !base::CreateDirectory(*local_prefs_dir)) {
    return nullptr;
  }

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());
  pref_registry->RegisterBooleanPref(kPrefQualified, false);
  RegisterPersistedDataPrefs(pref_registry);

  std::unique_ptr<PrefService> pref_service(
      CreatePrefService(*local_prefs_dir, pref_registry, kCreatePrefsWait));
  if (!pref_service) {
    return nullptr;
  }

  return base::MakeRefCounted<UpdaterPrefsImpl>(*local_prefs_dir, nullptr,
                                                std::move(pref_service));
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
