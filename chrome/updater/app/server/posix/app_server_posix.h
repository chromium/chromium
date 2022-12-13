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

class AppServerPosix : public AppServer {
 public:
  AppServerPosix();

  void TaskStarted();
  void TaskCompleted();

 protected:
  ~AppServerPosix() override;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<UpdateServiceInternalStub> active_duty_internal_stub_;

 private:
  void MarkTaskStarted();
  void AcknowledgeTaskCompletion();

  int tasks_running_ = 0;
  // Task runner bound to the main sequence and the update service instance.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
};

}  // namespace updater

#endif  // CHROME_UPDATER_APP_SERVER_POSIX_APP_SERVER_POSIX_H_
