// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl_qualifying.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/updater/check_for_updates_task.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/update_service_internal.h"
#include "components/prefs/pref_service.h"

namespace updater {
namespace {

constexpr char kQualificationInitialVersion[] = "0.1";

class UpdateServiceInternalQualifyingImpl : public UpdateServiceInternal {
 public:
  UpdateServiceInternalQualifyingImpl(scoped_refptr<Configurator> config,
                                      scoped_refptr<LocalPrefs> local_prefs)
      : config_(config), local_prefs_(local_prefs) {}

  // Overrides for updater::UpdateServiceInternal.
  void Run(base::OnceClosure callback) override {
    VLOG(1) << __func__ << " (Qualifying)";
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    scoped_refptr<UpdateServiceImpl> service =
        base::MakeRefCounted<UpdateServiceImpl>(config_);

    RegistrationRequest registration;
    registration.app_id = kQualificationAppId;
    registration.version = base::Version(kQualificationInitialVersion);

    service->RegisterApp(
        registration,
        base::BindOnce(
            &UpdateServiceInternalQualifyingImpl::RegisterQualificationAppDone,
            this, std::move(callback)));
  }

  void Hello(base::OnceClosure callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__ << " (Qualifying)";
    std::move(callback).Run();
  }

 private:
  ~UpdateServiceInternalQualifyingImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void RegisterQualificationAppDone(base::OnceClosure callback, int result) {
    // Create a `CheckForUpdatesTask` with the local prefs' config and perform
    // an `Update` task for `kQualificationAppId`.
    VLOG(2) << "Registration response: " << result;
    base::MakeRefCounted<CheckForUpdatesTask>(
        config_,
        base::BindOnce(&UpdateServiceImpl::Update,
                       base::MakeRefCounted<UpdateServiceImpl>(config_),
                       kQualificationAppId, "",
                       UpdateService::Priority::kBackground,
                       UpdateService::PolicySameVersionUpdate::kNotAllowed,
                       base::DoNothing()))
        ->Run(base::BindOnce(
            &UpdateServiceInternalQualifyingImpl::QualificationDone, this,
            std::move(callback)));
  }

  void QualificationDone(base::OnceClosure callback) {
    auto persisted_data =
        base::MakeRefCounted<PersistedData>(local_prefs_->GetPrefService());
    const base::Version qualification_app_version =
        persisted_data->GetProductVersion(kQualificationAppId);
    VLOG(0) << "qualification_app_version: " << qualification_app_version;
    const bool qualification_app_version_updated =
        qualification_app_version.CompareTo(
            base::Version(kQualificationInitialVersion)) == 1;

    local_prefs_->SetQualified(qualification_app_version_updated);
    local_prefs_->GetPrefService()->CommitPendingWrite();

    std::move(callback).Run();
  }

  scoped_refptr<Configurator> config_;
  scoped_refptr<LocalPrefs> local_prefs_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

scoped_refptr<UpdateServiceInternal> MakeQualifyingUpdateServiceInternal(
    scoped_refptr<Configurator> config,
    scoped_refptr<LocalPrefs> local_prefs) {
  return base::MakeRefCounted<UpdateServiceInternalQualifyingImpl>(config,
                                                                   local_prefs);
}

}  // namespace updater
