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
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
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

  // Capture `update_service` to manage the object lifetime.
  scoped_refptr<UpdateService> update_service = CreateUpdateService();
  update_service->GetVersion(
      base::BindOnce(&AppInstall::GetVersionDone, this, update_service));
}

void AppInstall::GetVersionDone(scoped_refptr<UpdateService>,
                                const base::Version& version) {
  VLOG_IF(1, (version.IsValid()))
      << "Found active version: " << version.GetString();
  if (version.IsValid() && version >= base::Version(UPDATER_VERSION_STRING)) {
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
          base::BindOnce(&AppInstall::InstallCandidateDone, this)));
}

void AppInstall::InstallCandidateDone(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != 0) {
    Shutdown(result);
    return;
  }
  WakeCandidate();
}

void AppInstall::WakeCandidate() {
  // Invoke UpdateServiceInternal::InitializeUpdateService to wake this version
  // of the updater, qualify, and possibly promote this version as a result. The
  // |UpdateServiceInternal| instance has sequence affinity. Bind it in the
  // closure to ensure it is released in this sequence.
  scoped_refptr<UpdateServiceInternal> update_service_internal =
      CreateUpdateServiceInternal();
  update_service_internal->InitializeUpdateService(base::BindOnce(
      [](scoped_refptr<UpdateServiceInternal> /*update_service_internal*/,
         scoped_refptr<AppInstall> app_install) {
        app_install->WakeCandidateDone();
      },
      update_service_internal, base::WrapRefCounted(this)));
}

void AppInstall::MaybeInstallApp() {
  const std::string app_id = []() {
    // Returns the app id parsed from the tag, if the --tag is specified, or
    // the switch value of the --app-id command line argument.
    // Otherwise, returns an empty string.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    const std::string tag = command_line->GetSwitchValueASCII(kTagSwitch);
    if (!tag.empty()) {
      tagging::TagArgs tag_args;
      tagging::ErrorCode error = tagging::Parse(tag, base::nullopt, &tag_args);
      if (error == tagging::ErrorCode::kSuccess) {
        // TODO(crbug.com/1128631): support bundles. For now, assume one app.
        DCHECK_EQ(tag_args.apps.size(), size_t{1});
        const std::string& app_id = tag_args.apps.front().app_id;
        if (!app_id.empty()) {
          return app_id;
        }
      } else {
        VLOG(1) << "Tag parsing returned " << error << ".";
      }
    }
    return command_line->GetSwitchValueASCII(kAppIdSwitch);
  }();

  if (app_id.empty()) {
    Shutdown(0);
    return;
  }
  app_install_controller_ = app_install_controller_maker_.Run();
  app_install_controller_->InstallApp(
      app_id, base::BindOnce(&AppInstall::Shutdown, this));
}

}  // namespace updater
