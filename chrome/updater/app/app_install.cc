// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/lock.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_service.h"

namespace updater {

#if !BUILDFLAG(IS_WIN)
namespace {

class AppInstallControllerImpl : public AppInstallController {
 public:
  AppInstallControllerImpl() = default;

  // Override for AppInstallController.
  void Initialize() override {}

  void InstallApp(const std::string& app_id,
                  const std::string& /*app_name*/,
                  base::OnceCallback<void(int)> callback) override {
    // TODO(crbug.com/40282228): Factor out common code from app_install_win.cc.
    RegistrationRequest request;
    request.app_id = app_id;
    request.version = base::Version(kNullVersion);
    std::optional<tagging::AppArgs> app_args = GetAppArgs(app_id);
    std::optional<tagging::TagArgs> tag_args = GetTagArgs().tag_args;
    if (app_args) {
      request.ap = app_args->ap;
    }
    if (tag_args) {
      request.brand_code = tag_args->brand_code;
    }
    update_service_->Install(request, GetDecodedInstallDataFromAppArgs(app_id),
                             GetInstallDataIndexFromAppArgs(app_id),
                             UpdateService::Priority::kForeground,
                             base::DoNothing(),
                             base::BindOnce([](UpdateService::Result result) {
                               return static_cast<int>(result);
                             }).Then(std::move(callback)));
  }

  void InstallAppOffline(const std::string& app_id,
                         const std::string& /*app_name*/,
                         base::OnceCallback<void(int)> callback) override {
    // TODO(crbug.com/40282228): Implement this.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), 0));
  }

  void Exit(int /*exit_code*/) override {}

  void set_update_service(
      scoped_refptr<UpdateService> update_service) override {
    update_service_ = update_service;
  }

 protected:
  ~AppInstallControllerImpl() override = default;

 private:
  scoped_refptr<UpdateService> update_service_;
};

}  // namespace

scoped_refptr<App> MakeAppInstall(bool /*is_silent_install*/) {
  return base::MakeRefCounted<AppInstall>(
      base::BindRepeating([]() -> scoped_refptr<AppInstallController> {
        return base::MakeRefCounted<AppInstallControllerImpl>();
      }));
}
#endif  // !BUILDFLAG(IS_WIN)

AppInstall::AppInstall(AppInstallController::Maker app_install_controller_maker)
    : app_install_controller_maker_(app_install_controller_maker),
      external_constants_(CreateExternalConstants()) {
  CHECK(app_install_controller_maker_);
}

AppInstall::~AppInstall() = default;

void AppInstall::Shutdown(int exit_code) {
  app_install_controller_->Exit(exit_code);
  App::Shutdown(exit_code);
}

int AppInstall::Initialize() {
  app_install_controller_ = app_install_controller_maker_.Run();
  app_install_controller_->Initialize();

  setup_lock_ =
      CreateScopedLock(kSetupMutex, updater_scope(), kWaitForSetupLock);
  return kErrorOk;
}

void AppInstall::FirstTaskRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (WrongUser(updater_scope())) {
    VLOG(0) << "The current user is not compatible with the current scope. "
            << (updater_scope() == UpdaterScope::kSystem
                    ? "Did you mean to run as admin/root?"
                    : "Did you mean to run as a non-admin/non-root user?");
    Shutdown(kErrorWrongUser);
    return;
  }

  if (!setup_lock_) {
    VLOG(0) << "Failed to acquire setup mutex; shutting down.";
    Shutdown(kErrorFailedToLockSetupMutex);
    return;
  }

  const TagParsingResult tag_parsing_result(GetTagArgs());

  // A tag parsing error is handled as an fatal error.
  if (tag_parsing_result.error != tagging::ErrorCode::kSuccess) {
    Shutdown(kErrorTagParsing);
    return;
  }
  const tagging::TagArgs tag_args =
      tag_parsing_result.tag_args.value_or(tagging::TagArgs());
  if (!tag_args.apps.empty()) {
    // Assume only one app is present since bundles are not supported.
    const tagging::AppArgs& app_args = tag_args.apps.front();
    app_id_ = app_args.app_id;
    app_name_ = app_args.app_name;
  } else {
    // If no apps are present, try to use --app-id, if present.
    app_id_ = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        kAppIdSwitch);
  }

  CreateUpdateServiceProxy();
  update_service_->GetVersion(
      base::BindOnce(&AppInstall::GetVersionDone, this));
}

void AppInstall::CreateUpdateServiceProxy() {
  update_service_ = updater::CreateUpdateServiceProxy(
      updater_scope(), external_constants_->OverinstallTimeout());
  app_install_controller_->set_update_service(update_service_);
}

void AppInstall::GetVersionDone(const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG_IF(1, version.IsValid()) << "Active version: " << version.GetString();
  if (version.IsValid() && version >= base::Version(kUpdaterVersion)) {
    MaybeInstallApp();
    return;
  }
  InstallCandidate(updater_scope(),
                   base::BindOnce(&AppInstall::InstallCandidateDone, this,
                                  version.IsValid()));
}

void AppInstall::InstallCandidateDone(bool valid_version, int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != 0) {
    Shutdown(result);
    return;
  }

  if (valid_version) {
    FetchPolicies();
    return;
  }

  // It's possible that a previous updater existed but is nonresponsive. In
  // this case, set the active version in global prefs so that this instance
  // will take over without qualification. Also, if EULA acceptance is still
  // required, record so here.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(
              [](UpdaterScope scope) {
                scoped_refptr<GlobalPrefs> prefs = CreateGlobalPrefs(scope);
                if (prefs) {
                  prefs->SetActiveVersion(kUpdaterVersion);
                  prefs->SetSwapping(true);
                  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                          kEulaRequiredSwitch)) {
                    base::MakeRefCounted<PersistedData>(
                        scope, prefs->GetPrefService(), nullptr)
                        ->SetEulaRequired(true);
                  }
                  PrefsCommitPendingWrites(prefs->GetPrefService());
                }
              },
              updater_scope()),
          base::BindOnce(&AppInstall::WakeCandidate, this));
}

void AppInstall::WakeCandidate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invoke UpdateServiceInternal::Hello to wake this version of the updater,
  // qualify, and possibly promote this version as a result.
  CreateUpdateServiceInternalProxy(updater_scope())
      ->Hello(base::BindOnce(&AppInstall::CreateUpdateServiceProxy, this)
                  .Then(base::BindOnce(&AppInstall::FetchPolicies, this)));
}

void AppInstall::FetchPolicies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  update_service_->FetchPolicies(base::BindOnce(
      [](scoped_refptr<AppInstall> app_install, int result) {
        if (result != kErrorOk) {
          LOG(ERROR) << "FetchPolicies failed: " << result;
          app_install->Shutdown(result);
          return;
        }

        app_install->RegisterUpdater();
      },
      base::WrapRefCounted(this)));
}

void AppInstall::RegisterUpdater() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RegistrationRequest request;
  request.app_id = kUpdaterAppId;
  request.version = base::Version(kUpdaterVersion);
  update_service_->RegisterApp(
      request, base::BindOnce(
                   [](scoped_refptr<AppInstall> app_install, int result) {
                     if (result != kRegistrationSuccess) {
                       VLOG(2) << "Updater registration failed: " << result;
                       app_install->Shutdown(kErrorRegistrationFailed);
                       return;
                     }
                     app_install->MaybeInstallApp();
                   },
                   base::WrapRefCounted(this)));
}

void AppInstall::MaybeInstallApp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (app_id_.empty()) {
    Shutdown(kErrorOk);
    return;
  }
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(kOfflineDirSwitch)) {
    // Presence of "offlinedir" in command line indicates this is an offline
    // install. Note the check here is compatible with legacy command line
    // because `base::CommandLine::HasSwitch()` recognizes switches that
    // begin with '/' on Windows.
    app_install_controller_->InstallAppOffline(
        app_id_, app_name_, base::BindOnce(&AppInstall::Shutdown, this));

  } else {
    app_install_controller_->InstallApp(
        app_id_, app_name_, base::BindOnce(&AppInstall::Shutdown, this));
  }
}

}  // namespace updater
