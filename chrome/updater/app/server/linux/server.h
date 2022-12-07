// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_SERVER_LINUX_SERVER_H_
#define CHROME_UPDATER_APP_SERVER_LINUX_SERVER_H_

#include <memory>

#include "chrome/updater/app/server/posix/app_server_posix.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace updater {

class App;
class UpdateService;
class UpdateServiceInternal;
class UpdateServiceStub;

class AppServerLinux : public AppServerPosix {
 public:
  AppServerLinux();

 private:
  ~AppServerLinux() override;

  // Connects to the client and returns a message pipe which may be used to
  // instantiate a mojo receiver.
  mojo::ScopedMessagePipeHandle ConnectToClient();

  // Overrides for AppServer.
  void ActiveDuty(scoped_refptr<UpdateService> update_service) override;
  void ActiveDutyInternal(
      scoped_refptr<UpdateServiceInternal> update_service_internal) override;
  bool SwapInNewVersion() override;
  bool MigrateLegacyUpdaters(
      base::RepeatingCallback<void(const RegistrationRequest&)>
          register_callback) override;
  void UninstallSelf() override;

  std::unique_ptr<UpdateServiceStub> active_duty_stub_;
};

scoped_refptr<App> MakeAppServer();

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_LINUX_SERVER_H_
