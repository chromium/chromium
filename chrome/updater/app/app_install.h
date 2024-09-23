// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_INSTALL_H_
#define CHROME_UPDATER_APP_APP_INSTALL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/lock.h"

namespace base {
class Version;
}

namespace updater {

class ExternalConstants;
class UpdateService;

// This class defines an interface for installing an application. The interface
// is intended to be implemented for scenerios where UI and RPC calls to
// |UpdateService| are involved, hence the word `controller` in the name of
// the interface.
class AppInstallController
    : public base::RefCountedThreadSafe<AppInstallController> {
 public:
  using Maker = base::RepeatingCallback<scoped_refptr<AppInstallController>()>;
  virtual void Initialize() = 0;

  virtual void InstallApp(const std::string& app_id,
                          const std::string& app_name,
                          base::OnceCallback<void(int)> callback) = 0;

  virtual void InstallAppOffline(const std::string& app_id,
                                 const std::string& app_name,
                                 base::OnceCallback<void(int)> callback) = 0;
  virtual void Exit(int exit_code) = 0;

  virtual void set_update_service(
      scoped_refptr<UpdateService> update_service) = 0;

 protected:
  virtual ~AppInstallController() = default;

 private:
  friend class base::RefCountedThreadSafe<AppInstallController>;
};

// Sets the updater up then installs an application while displaying the UI
// progress window.
class AppInstall : public App {
 public:
  explicit AppInstall(AppInstallController::Maker app_install_controller_maker);

 private:
  ~AppInstall() override;

  void Shutdown(int exit_code);

  // Overrides for App.
  [[nodiscard]] int Initialize() override;
  void FirstTaskRun() override;

  // Initializes or reinitializes `update_service_`. Reinitialization can be
  // used to pick up a possible change to the active updater.
  void CreateUpdateServiceProxy();

  // Called after the version of the active updater has been retrieved.
  void GetVersionDone(const base::Version& version);

  void InstallCandidateDone(bool valid_version, int result);

  void WakeCandidate();

  void FetchPolicies();

  void RegisterUpdater();

  // Installs an application if the `app_id_` is valid.
  void MaybeInstallApp();

  // Bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Inter-process lock taken by AppInstall, AppUninstall, and AppUpdate.
  std::unique_ptr<ScopedLock> setup_lock_;

  // The `app_id_` is parsed from the tag, if the the tag is present, or from
  // the command line argument --app-id.
  std::string app_id_;
  std::string app_name_;

  // Creates instances of |AppInstallController|.
  AppInstallController::Maker app_install_controller_maker_;

  scoped_refptr<AppInstallController> app_install_controller_;

  scoped_refptr<ExternalConstants> external_constants_;

  scoped_refptr<UpdateService> update_service_;
};

scoped_refptr<App> MakeAppInstall(bool is_silent_install);

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_INSTALL_H_
