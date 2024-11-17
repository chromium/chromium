// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl_qualifying.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
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
constexpr char kQualificationUpdatesSuppressedVersion[] = "0.2";

class UpdateServiceInternalQualifyingImpl : public UpdateServiceInternal {
 public:
  UpdateServiceInternalQualifyingImpl(scoped_refptr<Configurator> config,
                                      scoped_refptr<LocalPrefs> local_prefs)
      : config_(config), local_prefs_(local_prefs) {}

  // Overrides for updater::UpdateServiceInternal.
  void Run(base::OnceClosure callback) override {
    VLOG(1) << __func__ << " (Qualifying)";
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Qualify(std::move(callback));
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

  void Qualify(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (attempted_qualification_) {
      std::move(callback).Run();
      return;
    }
    attempted_qualification_ = true;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
        base::BindOnce(&DoPlatformSpecificHealthChecks, GetUpdaterScope()),
        base::BindOnce(
            &UpdateServiceInternalQualifyingImpl::
                PlatformSpecificHealthChecksDone,
            this,
            base::BindOnce(
                &UpdateServiceInternalQualifyingImpl::QualificationDone, this,
                std::move(callback))));
  }

  void PlatformSpecificHealthChecksDone(base::OnceCallback<void(bool)> callback,
                                        bool success) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!success) {
      VLOG(1) << "Platfom-specific qualification checks failed.";
      std::move(callback).Run(false);
      return;
    }
    RegisterQualificationApp(std::move(callback));
  }

  void RegisterQualificationApp(base::OnceCallback<void(bool)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    RegistrationRequest registration;
    registration.app_id = kQualificationAppId;

    // If the update check period is set to zero, the updater is pre-qualified
    // by registering a higher version of the qualification app. This is because
    // the qualification app update will not happen if the update check period
    // is set to zero.
    registration.version =
        base::Version(config_->NextCheckDelay().is_zero()
                          ? kQualificationUpdatesSuppressedVersion
                          : kQualificationInitialVersion);
    base::MakeRefCounted<UpdateServiceImpl>(GetUpdaterScope(), config_)
        ->RegisterApp(registration,
                      base::BindOnce(&UpdateServiceInternalQualifyingImpl::
                                         RegisterQualificationAppDone,
                                     this, std::move(callback)));
  }

  void RegisterQualificationAppDone(base::OnceCallback<void(bool)> callback,
                                    int result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (result != kRegistrationSuccess) {
      VLOG(1) << "Registration failed: " << result;
      std::move(callback).Run(false);
      return;
    }
    UpdateCheck(std::move(callback));
  }

  void UpdateCheck(base::OnceCallback<void(bool)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Create a `CheckForUpdatesTask` with the local prefs' config and perform
    // an `Update` task for `kQualificationAppId`.
    base::MakeRefCounted<CheckForUpdatesTask>(
        config_, GetUpdaterScope(),
        /*task_name=*/"Update(kQualificationAppId)",
        base::BindOnce(
            &UpdateServiceImpl::Update,
            base::MakeRefCounted<UpdateServiceImpl>(GetUpdaterScope(), config_),
            base::ToLowerASCII(kQualificationAppId), "",
            UpdateService::Priority::kBackground,
            UpdateService::PolicySameVersionUpdate::kNotAllowed,
            base::DoNothing()))
        ->Run(base::BindOnce(
            &UpdateServiceInternalQualifyingImpl::UpdateCheckDone, this,
            std::move(callback)));
  }

  void UpdateCheckDone(base::OnceCallback<void(bool)> callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const base::Version qualification_app_version =
        config_->GetPersistedData()->GetProductVersion(kQualificationAppId);
    VLOG(2) << "qualification_app_version: " << qualification_app_version;
    std::move(callback).Run(qualification_app_version.CompareTo(base::Version(
                                kQualificationInitialVersion)) == 1);
  }

  void QualificationDone(base::OnceClosure callback, bool qualified) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << "Qualification complete, qualified = " << qualified;
    local_prefs_->SetQualified(qualified);
    local_prefs_->GetPrefService()->CommitPendingWrite();
    std::move(callback).Run();
  }

  scoped_refptr<Configurator> config_;
  scoped_refptr<LocalPrefs> local_prefs_;
  bool attempted_qualification_ = false;

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
