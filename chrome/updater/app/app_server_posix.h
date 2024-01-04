// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_SERVER_POSIX_H_
#define CHROME_UPDATER_APP_APP_SERVER_POSIX_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/app/app_server.h"

namespace updater {

class UpdateServiceInternalStub;
class UpdateServiceStub;

class AppServerPosix : public AppServer {
 public:
  AppServerPosix();

 protected:
  ~AppServerPosix() override;

  // Overrides of AppServer.
  void ActiveDuty(scoped_refptr<UpdateService> update_service) override;

 private:
  void ActiveDutyInternal(
      scoped_refptr<UpdateServiceInternal> update_service_internal) override;
  bool SwapInNewVersion() override;
  void RepairUpdater(UpdaterScope scope, bool is_internal) override;
  void UninstallSelf() override;
  void Uninitialize() override;
  bool ShutdownIfIdleAfterTask() override;
  void OnDelayedTaskComplete() override;

  std::unique_ptr<UpdateServiceInternalStub> active_duty_internal_stub_;
  std::unique_ptr<UpdateServiceStub> active_duty_stub_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_SERVER_POSIX_H_
