// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_install.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/control_service.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/updater_version.h"
#include "components/prefs/pref_service.h"

namespace updater {

#if !defined(OS_WIN)
namespace {

class SplashScreenImpl : public SplashScreen {
 public:
  // Overrides for SplashScreen.
  void Show() override {}
  void Dismiss(base::OnceClosure callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
  }
};

class AppInstallControllerImpl : public AppInstallController {
 public:
  // Override for AppInstallController.
  void InstallApp(const std::string& app_id,
                  base::OnceCallback<void(int)> callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), 0));
  }

 protected:
  ~AppInstallControllerImpl() override = default;
};

}  // namespace

scoped_refptr<App> MakeAppInstall() {
  return base::MakeRefCounted<AppInstall>(
      base::BindRepeating([]() -> std::unique_ptr<SplashScreen> {
        return std::make_unique<SplashScreenImpl>();
      }),
      base::BindRepeating([]() -> scoped_refptr<AppInstallController> {
        return base::MakeRefCounted<AppInstallControllerImpl>();
      }));
}
#endif  // !defined(OS_WIN)

AppInstall::AppInstall(SplashScreen::Maker splash_screen_maker,
                       AppInstallController::Maker app_install_controller_maker)
    : splash_screen_maker_(std::move(splash_screen_maker)),
      app_install_controller_maker_(app_install_controller_maker) {
  DCHECK(splash_screen_maker_);
  DCHECK(app_install_controller_maker_);
}

AppInstall::~AppInstall() = default;

void AppInstall::Initialize() {
  base::i18n::InitializeICU();

  // Creating |global_prefs_| requires acquiring a global lock, and this lock is
  // typically owned by the RPC server. That means that if the server is
  // running, the following code will block, and the install will not proceed
  // until the server releases the lock.
  global_prefs_ = CreateGlobalPrefs();
  local_prefs_ = CreateLocalPrefs();
}

void AppInstall::FirstTaskRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  splash_screen_ = splash_screen_maker_.Run();
  splash_screen_->Show();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce([]() { return InstallCandidate(false); }),
      base::BindOnce(
          [](SplashScreen* splash_screen, base::OnceCallback<void(int)> done,
             int result) {
            splash_screen->Dismiss(base::BindOnce(std::move(done), result));
          },
          splash_screen_.get(),
          base::BindOnce(&AppInstall::InstallCandidateDone, this)));
}

// Updates the prefs if installing the updater is successful, then continue
// installing the application if --app-id is specified on the command line.
void AppInstall::InstallCandidateDone(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != 0) {
    Shutdown(result);
    return;
  }

  // Invoke |HandleAppId| to continue the execution flow.
  MakeActive(base::BindOnce(&AppInstall::HandleAppId, this));
}

void AppInstall::HandleAppId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This releases the prefs lock, and the RPC server can be started.
  global_prefs_ = nullptr;
  local_prefs_ = nullptr;

  // If no app id is provided, then invoke ControlService::Run to wake
  // this version of the updater, do an update check, and possibly promote
  // this version as a result.
  const std::string app_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kAppIdSwitch);
  if (app_id.empty()) {
    // The instance of |CreateControlService| has sequence affinity. Bind it
    // in the closure to ensure it is released in this sequence.
    scoped_refptr<ControlService> control_service = CreateControlService();
    control_service->Run(base::BindOnce(
        [](scoped_refptr<ControlService> /*control_service*/,
           scoped_refptr<AppInstall> app_install) { app_install->Shutdown(0); },
        control_service, base::WrapRefCounted(this)));
    return;
  }

  app_install_controller_ = app_install_controller_maker_.Run();
  app_install_controller_->InstallApp(
      app_id, base::BindOnce(&AppInstall::Shutdown, this));
}

// TODO(crbug.com/1109231) - this is a temporary workaround.
void AppInstall::MakeActive(base::OnceClosure done) {
  local_prefs_->SetQualified(true);
  local_prefs_->GetPrefService()->CommitPendingWrite(base::BindOnce(
      [](base::OnceClosure done, PrefService* pref_service) {
        DCHECK(pref_service);
        auto persisted_data = base::MakeRefCounted<PersistedData>(pref_service);
        persisted_data->SetProductVersion(
            kUpdaterAppId, base::Version(UPDATER_VERSION_STRING));
        pref_service->CommitPendingWrite(std::move(done));
      },
      std::move(done), global_prefs_->GetPrefService()));
}

}  // namespace updater
