// Copyright 2020 The Chromium Authors
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
  // A ref to this object is held until the registration completes. Must be
  // called on the sequence on which the BrowserUpdateClient was created.
  void Register();

  // Triggers an on-demand update from the Chromium updater, reporting status
  // updates to the callback. A ref to this object is held until the update
  // completes. Must be called on the sequence on which the BrowserUpdateClient
  // was created. `version_updater_callback` will be run on the same sequence.
  void CheckForUpdate(
      updater::UpdateService::StateChangeCallback version_updater_callback);

  // Launches the updater to run its periodic background tasks. This is a
  // mechanism to act as a backup periodic scheduler for the updater.
  void RunPeriodicTasks(base::OnceClosure callback);

  // Gets the current updater version. Can also be used to check for the
  // existence of the updater. A ref to the BrowserUpdaterClient is held until
  // the callback is invoked. Must be called on the sequence on which the
  // BrowserUpdateClient was created. `callback` will be run on the same
  // sequence.
  void GetUpdaterVersion(base::OnceCallback<void(const std::string&)> callback);

 protected:
  friend class base::RefCountedThreadSafe<BrowserUpdaterClient>;
  BrowserUpdaterClient();
  virtual ~BrowserUpdaterClient();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Helper method for Register() to be implemented by each platform to initiate
  // the registration. Runs in the thread pool.
  virtual void BeginRegister(const std::string& version,
                             updater::UpdateService::Callback callback) = 0;

  // Helper method for RunPeriodicTasks() to be implemented by each platform.
  // Runs in the thread pool.
  virtual void BeginRunPeriodicTasks(base::OnceClosure callback) = 0;

  // Helper method for CheckForUpdate() to be implemented by each platform to
  // initiate on-demand updates. Runs in the thread pool.
  virtual void BeginUpdateCheck(
      updater::UpdateService::StateChangeCallback state_change,
      updater::UpdateService::Callback callback) = 0;

  // Platform-specific helper for GetUpdaterVersion. Runs in the thread pool.
  virtual void BeginGetUpdaterVersion(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Handles status update from Chromium updater when registration is completed.
  void RegistrationCompleted(updater::UpdateService::Result result);

  // Handles the completion of RunPeriodicTasks.
  void RunPeriodicTasksCompleted(base::OnceClosure callback);

  // Handles status update from Chromium updater when updates are completed.
  void UpdateCompleted(updater::UpdateService::StateChangeCallback callback,
                       updater::UpdateService::Result result);

  void GetUpdaterVersionCompleted(
      base::OnceCallback<void(const std::string&)> callback,
      const std::string& version);
};

#endif  // CHROME_BROWSER_UPDATER_BROWSER_UPDATER_CLIENT_H_
