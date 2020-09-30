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
#include "base/logging.h"
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
#include "chrome/updater/registration_data.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/update_service.h"
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
}

void AppInstall::FirstTaskRun() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());

  splash_screen_ = splash_screen_maker_.Run();
  splash_screen_->Show();

  InstallCandidate(
      false,
      base::BindOnce(
          [](SplashScreen* splash_screen, base::OnceCallback<void(int)> done,
             int result) {
            splash_screen->Dismiss(base::BindOnce(std::move(done), result));
          },
          splash_screen_.get(),
          base::BindOnce(&AppInstall::InstallCandidateDone, this)));
}

void AppInstall::InstallCandidateDone(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != 0) {
    Shutdown(result);
    return;
  }

  // Invoke ControlService::InitializeUpdateService to wake this version of the
  // updater, qualify, and possibly promote this version as a result. The
  // instance of |CreateControlService| has sequence affinity. Bind it in the
  // closure to ensure it is released in this sequence.
  scoped_refptr<ControlService> control_service = CreateControlService();
  control_service->InitializeUpdateService(base::BindOnce(
      [](scoped_refptr<ControlService> /*control_service*/,
         scoped_refptr<AppInstall> app_install) {
        app_install->RegisterUpdater();
      },
      control_service, base::WrapRefCounted(this)));
}

void AppInstall::RegisterUpdater() {
  // TODO(crbug.com/1128060): We should update the updater's registration with
  // the new version, brand code, etc. For now, fake it.
  RegistrationResponse result;
  result.status_code = 0;
  RegisterUpdaterDone(result);
}

void AppInstall::RegisterUpdaterDone(const RegistrationResponse& response) {
  VLOG(1) << "Updater registration complete, code = " << response.status_code;
  HandleAppId();
}

void AppInstall::HandleAppId() {
  const std::string app_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kAppIdSwitch);
  if (app_id.empty()) {
    Shutdown(0);
    return;
  }
  app_install_controller_ = app_install_controller_maker_.Run();
  app_install_controller_->InstallApp(
      app_id, base::BindOnce(&AppInstall::Shutdown, this));
}

}  // namespace updater
