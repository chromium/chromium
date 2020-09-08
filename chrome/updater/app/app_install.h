// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_INSTALL_H_
#define CHROME_UPDATER_APP_APP_INSTALL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/splash_screen.h"

namespace base {
class TaskRunner;
}

namespace updater {

class LocalPrefs;
class GlobalPrefs;

// This class defines an interface for installing an application. The interface
// is intended to be implemented for scenerios where UI and RPC calls to
// |UpdateService| are involved, hence the word `controller` in the name of
// the ]interface.
class AppInstallController
    : public base::RefCountedThreadSafe<AppInstallController> {
 public:
  using Maker = base::RepeatingCallback<scoped_refptr<AppInstallController>()>;
  virtual void InstallApp(const std::string& app_id,
                          base::OnceCallback<void(int)> callback) = 0;

 protected:
  virtual ~AppInstallController() = default;

 private:
  friend class base::RefCountedThreadSafe<AppInstallController>;
};

// Sets the updater up, shows up a splash screen, then installs an application
// while displaying the UI progress window.
class AppInstall : public App {
 public:
  AppInstall(SplashScreen::Maker splash_screen_maker,
             AppInstallController::Maker app_install_controller_maker);

 private:
  ~AppInstall() override;

  // Overrides for App.
  void Initialize() override;
  void FirstTaskRun() override;

  void InstallCandidateDone(int result);

  // Handles the --app-id command line argument, and triggers installing of the
  // corresponding app-id if the argument is present.
  void HandleAppId();

  // Makes this version of the updater active, self-registers for updates, then
  // runs the |done| closure.
  void MakeActive(base::OnceClosure done);

  // Bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Creates instances of |SplashScreen|.
  SplashScreen::Maker splash_screen_maker_;

  // Creates instances of |AppInstallController|.
  AppInstallController::Maker app_install_controller_maker_;

  // The splash screen has a fading effect. That means that the splash screen
  // needs to be alive for a while, until the fading effect is over.
  std::unique_ptr<SplashScreen> splash_screen_;

  scoped_refptr<AppInstallController> app_install_controller_;

  // These prefs objects are used to make the updater active and register this
  // version of the updater for self-updates.
  //
  // TODO(crbug.com/1109231) - this is a temporary workaround until a better
  // fix is found.
  std::unique_ptr<LocalPrefs> local_prefs_;
  std::unique_ptr<GlobalPrefs> global_prefs_;

  scoped_refptr<base::TaskRunner> make_active_task_runner_;
};

scoped_refptr<App> MakeAppInstall();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_INSTALL_H_
