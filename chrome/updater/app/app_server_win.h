// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_SERVER_WIN_H_
#define CHROME_UPDATER_APP_APP_SERVER_WIN_H_

#include <windows.h>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/app/app_server.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

// Returns S_OK if user install, or if the COM caller is admin. Error otherwise.
HRESULT IsCOMCallerAllowed();

// This class is responsible for the lifetime of the COM server, as well as
// class factory registration.
//
// The instance of the this class is managed by a singleton and it leaks at
// runtime.
class AppServerWin : public AppServer {
 public:
  AppServerWin();

  using AppServer::config;
  using AppServer::prefs;

  // Posts the `task` to the sequence bound to this instance.
  static void PostRpcTask(base::OnceClosure task);

  scoped_refptr<UpdateService> update_service() { return update_service_; }

  scoped_refptr<UpdateServiceInternal> update_service_internal() {
    return update_service_internal_;
  }

  // Handles COM factory unregistration then triggers program shutdown. This
  // function runs on a COM RPC thread when the WRL module is destroyed.
  void Stop();

 private:
  ~AppServerWin() override;

  // Called before each invocation of an `UpdateService` or
  // `UpdateServiceInternal` method. Increments the WRL Module count.
  void TaskStarted();

  // Overrides for AppServer.
  void ActiveDuty(scoped_refptr<UpdateService> update_service) override;
  void ActiveDutyInternal(
      scoped_refptr<UpdateServiceInternal> update_service_internal) override;
  bool SwapInNewVersion() override;
  void RepairUpdater(UpdaterScope scope, bool is_internal) override;
  void UninstallSelf() override;
  bool ShutdownIfIdleAfterTask() override;

  // Decrements the WRL Module count.
  void OnDelayedTaskComplete() override;

  // Registers and unregisters the out-of-process COM class factories.
  HRESULT RegisterClassObjects();
  HRESULT RegisterInternalClassObjects();
  void UnregisterClassObjects();

  // Waits until the last COM object is released.
  void WaitForExitSignal();

  // Called when the last object is released.
  void SignalExit();

  // Creates an out-of-process WRL Module.
  void CreateWRLModule();

  // Handles COM setup and registration.
  void Start(base::OnceCallback<HRESULT()> register_callback);

  void PostRpcTaskOnMainSequence(base::OnceClosure task);

  // Task runner bound to the main sequence.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();

  // These services run the in-process code, which is delegating to the
  // |update_client| component.
  scoped_refptr<UpdateService> update_service_;
  scoped_refptr<UpdateServiceInternal> update_service_internal_;
};

// Returns the singleton AppServerWin instance.
scoped_refptr<AppServerWin> GetAppServerWinInstance();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_SERVER_WIN_H_
