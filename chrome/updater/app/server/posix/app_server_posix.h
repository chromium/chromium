// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_POSIX_APP_SERVER_POSIX_H_
#define CHROME_UPDATER_APP_SERVER_POSIX_APP_SERVER_POSIX_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/updater/app/app_server.h"

namespace updater {

class UpdateServiceInternalStub;
class UpdateServiceStub;

class AppServerPosix : public AppServer {
 public:
  AppServerPosix();

 protected:
  // Overrides of App.
  ~AppServerPosix() override;
  // Overrides of AppServer.
  void ActiveDuty(scoped_refptr<UpdateService> update_service) override;

 private:
  base::TimeDelta ServerKeepAlive();
  void TaskStarted();
  void TaskCompleted();
  void MarkTaskStarted();
  void AcknowledgeTaskCompletion();

  // Overrides of AppServer.
  void ActiveDutyInternal(
      scoped_refptr<UpdateServiceInternal> update_service_internal) override;
  bool SwapInNewVersion() override;
  bool MigrateLegacyUpdaters(
      base::RepeatingCallback<void(const RegistrationRequest&)>
          register_callback) override;
  void UninstallSelf() override;
  void Uninitialize() override;

  std::unique_ptr<UpdateServiceInternalStub> active_duty_internal_stub_;
  std::unique_ptr<UpdateServiceStub> active_duty_stub_;
  int tasks_running_ = 0;
  // Task runner bound to the main sequence and the update service instance.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  SEQUENCE_CHECKER(sequence_checker_);
};

scoped_refptr<App> MakeAppServer();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_POSIX_APP_SERVER_POSIX_H_
