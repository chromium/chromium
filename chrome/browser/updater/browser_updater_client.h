// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class Version;
}

// Cross-platform client to communicate between the browser and the Chromium
// updater. It helps the browser register to the Chromium updater and invokes
// on-demand updates.
class BrowserUpdaterClient
    : public base::RefCountedThreadSafe<BrowserUpdaterClient> {
 public:
  // Must be called on the program's main sequence.
  static scoped_refptr<BrowserUpdaterClient> Create(
      updater::UpdaterScope scope);
  static scoped_refptr<BrowserUpdaterClient> Create(
      base::RepeatingCallback<scoped_refptr<updater::UpdateService>()>
          proxy_provider,
      updater::UpdaterScope scope);

  explicit BrowserUpdaterClient(
      scoped_refptr<updater::UpdateService> update_service);

  // Registers the browser to the Chromium updater via IPC registration API.
  // When registration is completed, it will call RegistrationCompleted().
  // A ref to this object is held until the registration completes. Must be
  // called on the sequence on which the BrowserUpdateClient was created.
  // `complete` will be called after registration on the same sequence.
  void Register(base::OnceClosure complete);

  // Triggers an on-demand update from the Chromium updater, reporting status
  // updates to the callback. A ref to this object is held until the update
  // completes. Must be called on the sequence on which the BrowserUpdateClient
  // was created. `version_updater_callback` will be run on the same sequence.
  void CheckForUpdate(
      base::RepeatingCallback<void(const updater::UpdateService::UpdateState&)>
          version_updater_callback);

  // Launches the updater to run its periodic background tasks. This is a
  // mechanism to act as a backup periodic scheduler for the updater.
  void RunPeriodicTasks(base::OnceClosure callback);

  // Gets the current updater version. Can also be used to check for the
  // existence of the updater. A ref to the BrowserUpdaterClient is held until
  // the callback is invoked. Must be called on the sequence on which the
  // BrowserUpdateClient was created. `callback` will be run on the same
  // sequence.
  void GetUpdaterVersion(
      base::OnceCallback<void(const base::Version&)> callback);

  // Returns whether the browser is registered with the updater. A ref to the
  // BrowserUpdaterClient is held until the callback is invoked. Must be called
  // on the sequence on which the BrowserUpdaterClient was created. `callback`
  // will be run on the same sequence.
  void IsBrowserRegistered(base::OnceCallback<void(bool)> callback);

 protected:
  friend class base::RefCountedThreadSafe<BrowserUpdaterClient>;
  virtual ~BrowserUpdaterClient();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  static std::string GetAppId();
  static bool AppMatches(const updater::UpdateService::AppState& app);
  updater::RegistrationRequest GetRegistrationRequest();

  void RegistrationCompleted(base::OnceClosure complete, int result);
  void GetUpdaterVersionCompleted(
      base::OnceCallback<void(const base::Version&)> callback,
      const base::Version& version);
  void UpdateCompleted(
      base::RepeatingCallback<void(const updater::UpdateService::UpdateState&)>
          callback,
      updater::UpdateService::Result result);
  void RunPeriodicTasksCompleted(base::OnceClosure callback);
  void IsBrowserRegisteredCompleted(
      base::OnceCallback<void(bool)> callback,
      const std::vector<updater::UpdateService::AppState>& apps);

  template <updater::UpdaterScope scope>
  static scoped_refptr<BrowserUpdaterClient> GetClient(
      base::RepeatingCallback<scoped_refptr<updater::UpdateService>()>
          proxy_provider);

  scoped_refptr<updater::UpdateService> update_service_;
  base::WeakPtrFactory<BrowserUpdaterClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_
