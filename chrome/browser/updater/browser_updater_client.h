// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/mojom/updater_service.mojom.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class Version;
class FilePath;
}  // namespace base

namespace updater {

// Cross-platform client to communicate between the browser and the Chromium
// updater. It helps the browser register to the Chromium updater and invokes
// on-demand updates.
class BrowserUpdaterClient
    : public base::RefCountedThreadSafe<BrowserUpdaterClient> {
 public:
  // Must be called on the program's main sequence.
  static scoped_refptr<BrowserUpdaterClient> Create(UpdaterScope scope);
  static scoped_refptr<BrowserUpdaterClient> Create(
      base::RepeatingCallback<scoped_refptr<UpdateService>()> proxy_provider,
      UpdaterScope scope);

  explicit BrowserUpdaterClient(scoped_refptr<UpdateService> update_service);

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
      base::RepeatingCallback<void(const UpdateService::UpdateState&)>
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

  // Queries the current state of the updater.
  void GetUpdaterState(
      base::OnceCallback<void(const UpdateService::UpdaterState&)> callback);

  // Gets the current enterprise policies for the updater as a JSON blob.
  void GetPoliciesJson(base::OnceCallback<void(const std::string&)> callback);

  // Retrieves metadata about applications managed by the updater.
  void GetAppStates(
      base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback);

  // Returns the browser's app ID. App IDs are case-insensitive and it may not
  // be in the same case used elsewhere in the browser.
  static std::string GetAppId();

  // Returns the expected existence checker path for this browser instance.
  static base::FilePath GetExpectedEcp();

 protected:
  friend class base::RefCountedThreadSafe<BrowserUpdaterClient>;
  virtual ~BrowserUpdaterClient();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  static bool AppMatches(const UpdateService::AppState& app);
  RegistrationRequest GetRegistrationRequest();

  // Methods which call into updater_service_ should have a "Completed" function
  // to ensure that the BrowserUpdaterClient outlives the request. Otherwise,
  // sequential calls from methods in updater.h can cause multiple service
  // proxies to be instantiated, which causes interference and dropped messages.
  void RegistrationCompleted(base::OnceClosure complete, int result);
  void GetUpdaterVersionCompleted(
      base::OnceCallback<void(const base::Version&)> callback,
      const base::Version& version);
  void UpdateCompleted(
      base::RepeatingCallback<void(const UpdateService::UpdateState&)> callback,
      UpdateService::Result result);
  void RunPeriodicTasksCompleted(base::OnceClosure callback);
  void IsBrowserRegisteredCompleted(
      base::OnceCallback<void(bool)> callback,
      const std::vector<UpdateService::AppState>& apps);
  void GetUpdaterStateCompleted(
      base::OnceCallback<void(const UpdateService::UpdaterState&)> callback,
      const UpdateService::UpdaterState& updater_state);
  void GetPoliciesJsonCompleted(
      base::OnceCallback<void(const std::string&)> callback,
      const std::string& policies);
  void GetAppStatesCompleted(
      base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback,
      const std::vector<mojom::AppState>& app_states);

  template <UpdaterScope scope>
  static scoped_refptr<BrowserUpdaterClient> GetClient(
      base::RepeatingCallback<scoped_refptr<UpdateService>()> proxy_provider);

  scoped_refptr<UpdateService> update_service_;
  base::WeakPtrFactory<BrowserUpdaterClient> weak_ptr_factory_{this};
};

std::optional<mojom::AppState>& GetLastKnownBrowserRegistrationStorage();
std::optional<mojom::AppState>& GetLastKnownUpdaterRegistrationStorage();
std::optional<mojom::UpdateState>& GetLastOnDemandUpdateStateStorage();

}  // namespace updater

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_
