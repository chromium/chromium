// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_
#define CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

// Cross-platform client to communicate between the browser and the Chromium
// updater. It helps the browser register to the Chromium updater and invokes
// on-demand updates.
class BrowserUpdaterClient
    : public base::RefCountedThreadSafe<BrowserUpdaterClient> {
 public:
  static scoped_refptr<BrowserUpdaterClient> Create(
      updater::UpdaterScope scope);

  // Registers the browser to the Chromium updater via IPC registration API.
  // When registration is completed, it will call RegistrationCompleted().
  // A ref to this object is held until the registration completes.
  void Register();

  // Triggers an on-demand update from the Chromium updater, reporting status
  // updates to the callback. A ref to this object is held until the update
  // completes.
  void CheckForUpdate(
      base::RepeatingCallback<void(updater::UpdateService::UpdateState)>
          version_updater_callback);

  // Gets the current updater version. Can also be used to check for the
  // existence of the updater. A ref to the BrowserUpdaterClient is held until
  // the callback is invoked.
  virtual void GetUpdaterVersion(
      base::OnceCallback<void(const std::string&)> callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<BrowserUpdaterClient>;
  BrowserUpdaterClient();
  virtual ~BrowserUpdaterClient();

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return callback_task_runner_;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Helper method for Register() to be implemented by each platform to initiate
  // the registration.
  virtual void BeginRegister(const std::string& brand_code,
                             const std::string& tag,
                             const std::string& version,
                             updater::UpdateService::Callback callback) = 0;

  // Helper method for CheckForUpdate() to be implemented by each platform to
  // initiate on-demand updates.
  virtual void BeginUpdateCheck(
      updater::UpdateService::StateChangeCallback state_change,
      updater::UpdateService::Callback callback) = 0;

  // Handles status updates from the Chromium Updater during an on-demand
  // update. The updater::UpdateService::UpdateState is translated into a
  // VersionUpdater::StatusCallback.
  void HandleStatusUpdate(
      base::RepeatingCallback<void(updater::UpdateService::UpdateState)>
          callback,
      const updater::UpdateService::UpdateState& update_state);

  // Handles status update from Chromium updater when registration is completed.
  void RegistrationCompleted(updater::UpdateService::Result result);

  // Handles status update from Chromium updater when updates are completed.
  void UpdateCompleted(base::RepeatingCallback<
                           void(updater::UpdateService::UpdateState)> callback,
                       updater::UpdateService::Result result);

  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;
};

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_
