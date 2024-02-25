// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server_posix.h"

#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "chrome/updater/linux/systemd_util.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"

namespace updater {
namespace {

// Extends `AppServerPosix` to include a `SystemdService` which is enabled
// during `ActiveDuty`. This is required for the server to operate as a systemd
// daemon.
class AppServerLinux : public AppServerPosix {
 public:
  void ActiveDuty(scoped_refptr<UpdateService> update_service) override {
    systemd_service_ = base::SequenceBound<SystemdService>(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
    AppServerPosix::ActiveDuty(update_service);
  }

 private:
  ~AppServerLinux() override = default;

  base::SequenceBound<SystemdService> systemd_service_;
};

}  // namespace

void AppServerPosix::RepairUpdater(UpdaterScope scope, bool is_internal) {
  // No repairs to do on Linux.
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServerLinux>();
}

}  // namespace updater
