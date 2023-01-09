// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_SERVER_H_
#define CHROME_UPDATER_APP_APP_SERVER_H_

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/prefs.h"

namespace updater {

class UpdateServiceInternal;
class GlobalPrefs;
class UpdateService;
struct RegistrationRequest;

// AppServer runs as the updater server process. Multiple servers of different
// application versions can be run side-by-side. Each such server is called a
// "candidate". Exactly one candidate is "active" at any given time. Only the
// active candidate is permitted to mutate shared system state such as the
// global prefs file or versions of managed applications.
class AppServer : public App {
 public:
  AppServer();

 protected:
  ~AppServer() override;

  scoped_refptr<const ExternalConstants> external_constants() const {
    return external_constants_;
  }

  scoped_refptr<const UpdaterPrefs> prefs() const { return prefs_; }

  scoped_refptr<Configurator> config() const { return config_; }

  // Overrides of App.
  void Uninitialize() override;

 private:
  // Overrides of App.
  void Initialize() final;
  void FirstTaskRun() final;

  // Sets up the server to handle active version RPCs.
  virtual void ActiveDuty(scoped_refptr<UpdateService> update_service) = 0;

  // Sets up the server to handle internal service RPCs.
  virtual void ActiveDutyInternal(
      scoped_refptr<UpdateServiceInternal> update_service_internal) = 0;

  // Sets up all non-side-by-side registration to point to the new version.
  virtual bool SwapInNewVersion() = 0;

  // Imports metadata from legacy updaters, then replaces them with shims.
  virtual bool MigrateLegacyUpdaters(
      base::RepeatingCallback<void(const RegistrationRequest&)>
          register_callback) = 0;

  // Uninstalls this candidate version of the updater.
  virtual void UninstallSelf() = 0;

  // As part of initialization, an AppServer must do a mode check to determine
  // what mode of operation it should continue in. Possible modes include:
  //  - Qualify: this candidate is not yet qualified or active.
  //  - ActiveDuty: this candidate is the active candidate.
  //  - UninstallSelf: this candidate is older than the  active candidate.
  //  - Shutdown: none of the above.
  // If this candidate is already qualified but not yet active, or the state of
  // the system is consistent with an incomplete swap, ModeCheck may have the
  // side effect of promoting this candidate to the active candidate.
  base::OnceClosure ModeCheck();
  bool SwapVersions(GlobalPrefs* global_prefs);

  // Uninstalls the updater if it doesn't manage any apps, aside from itself.
  void MaybeUninstall();

  base::OnceClosure first_task_;
  scoped_refptr<ExternalConstants> external_constants_;
  scoped_refptr<UpdaterPrefs> prefs_;
  scoped_refptr<Configurator> config_;

  // If true, this version of the updater should uninstall itself during
  // shutdown.
  bool uninstall_self_ = false;

  // The number of times the server has started, as read from global prefs.
  int server_starts_ = 0;
};

scoped_refptr<App> AppServerInstance();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_SERVER_H_
