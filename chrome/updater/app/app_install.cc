// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

#if !BUILDFLAG(IS_WIN)
namespace {

class SplashScreenImpl : public SplashScreen {
 public:
  // Overrides for SplashScreen.
  void Show() override {}
  void Dismiss(base::OnceClosure callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
};

class AppInstallControllerImpl : public AppInstallController {
 public:
  explicit AppInstallControllerImpl(
      scoped_refptr<UpdateService> /*update_service*/) {}
  // Override for AppInstallController.
  void InstallApp(const std::string& /*app_id*/,
                  const std::string& /*app_name*/,
                  base::OnceCallback<void(int)> callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), 0));
  }

  void InstallAppOffline(const std::string& app_id,
                         const std::string& app_name,
                         base::OnceCallback<void(int)> callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), 0));
  }

 protected:
  ~AppInstallControllerImpl() override = default;
};

}  // namespace

scoped_refptr<App> MakeAppInstall(bool /*is_silent_install*/) {
  return base::MakeRefCounted<AppInstall>(
      base::BindRepeating(
          [](const std::string& /*app_name*/) -> std::unique_ptr<SplashScreen> {
            return std::make_unique<SplashScreenImpl>();
          }),
      base::BindRepeating([](scoped_refptr<UpdateService> update_service)
                              -> scoped_refptr<AppInstallController> {
        return base::MakeRefCounted<AppInstallControllerImpl>(update_service);
      }));
}
#endif  // !BUILDFLAG(IS_WIN)

AppInstall::AppInstall(SplashScreen::Maker splash_screen_maker,
                       AppInstallController::Maker app_install_controller_maker)
    : splash_screen_maker_(std::move(splash_screen_maker)),
      app_install_controller_maker_(app_install_controller_maker),
      external_constants_(CreateExternalConstants()) {
  DCHECK(splash_screen_maker_);
  DCHECK(app_install_controller_maker_);
}

AppInstall::~AppInstall() = default;

void AppInstall::FirstTaskRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::SequencedTaskRunner::HasCurrentDefault());

  const TagParsingResult tag_parsing_result =
      GetTagArgsForCommandLine(GetCommandLineLegacyCompatible());

  // A tag parsing error is handled as an fatal error.
  if (tag_parsing_result.error != tagging::ErrorCode::kSuccess) {
    Shutdown(kErrorTagParsing);
    return;
  }
  const tagging::TagArgs tag_args =
      tag_parsing_result.tag_args.value_or(tagging::TagArgs());
  if (!tag_args.apps.empty()) {
    // TODO(crbug.com/1128631): support bundles. For now, assume one app.
    DCHECK_EQ(tag_args.apps.size(), size_t{1});
    const tagging::AppArgs& app_args = tag_args.apps.front();
    app_id_ = app_args.app_id;
    app_name_ = app_args.app_name;
  } else {
    // If no apps are present, try to use --app-id, if present.
    app_id_ = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        kAppIdSwitch);
  }

  splash_screen_ = splash_screen_maker_.Run(app_name_);
  splash_screen_->Show();

  // Creating instances of `UpdateServiceProxy` is possible only after task
  // scheduling has been initialized.
  update_service_ = CreateUpdateServiceProxy(
      updater_scope(), external_constants_->OverinstallTimeout());
  update_service_->GetVersion(
      base::BindOnce(&AppInstall::GetVersionDone, this));
}

void AppInstall::GetVersionDone(const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG_IF(1, (version.IsValid()))
      << "Found active version: " << version.GetString();
  if (version.IsValid() && version >= base::Version(kUpdaterVersion)) {
    splash_screen_->Dismiss(base::BindOnce(&AppInstall::MaybeInstallApp, this));
    return;
  }

  InstallCandidate(
      updater_scope(),
      base::BindOnce(
          [](SplashScreen* splash_screen, base::OnceCallback<void(int)> done,
             int result) {
            splash_screen->Dismiss(base::BindOnce(std::move(done), result));
          },
          splash_screen_.get(),
          base::BindOnce(&AppInstall::InstallCandidateDone, this,
                         version.IsValid())));
}

void AppInstall::InstallCandidateDone(bool valid_version, int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != 0) {
    Shutdown(result);
    return;
  }

  if (valid_version) {
    WakeCandidateDone();
    return;
  }

  // It's possible that a previous updater existed but is nonresponsive. In
  // this case, clear the active version in global prefs so that the system can
  // recover.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives()})
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(
              [](UpdaterScope scope) {
                scoped_refptr<GlobalPrefs> prefs = CreateGlobalPrefs(scope);
                prefs->SetActiveVersion("");
                PrefsCommitPendingWrites(prefs->GetPrefService());
              },
              updater_scope()),
          base::BindOnce(&AppInstall::WakeCandidate, this));
}

void AppInstall::WakeCandidate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invoke UpdateServiceInternal::Hello to wake this version of the updater,
  // qualify, and possibly promote this version as a result. The
  // |UpdateServiceInternal| instance has sequence affinity. Bind it in the
  // closure to ensure it is released in this sequence.
  scoped_refptr<UpdateServiceInternal> update_service_internal =
      CreateUpdateServiceInternalProxy(updater_scope());
  update_service_internal->Hello(base::BindOnce(
      [](scoped_refptr<UpdateServiceInternal> /*update_service_internal*/,
         scoped_refptr<AppInstall> app_install) {
        app_install->WakeCandidateDone();
      },
      update_service_internal, base::WrapRefCounted(this)));
}

void AppInstall::FetchPolicies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/1297163) - encapsulate the reinitialization of the
  // proxy server instance to avoid this special case.
  update_service_ = CreateUpdateServiceProxy(
      updater_scope(), external_constants_->OverinstallTimeout());
#endif

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
                     if (result != kRegistrationSuccess &&
                         result != kRegistrationAlreadyRegistered) {
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
  app_install_controller_ = app_install_controller_maker_.Run(update_service_);

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
