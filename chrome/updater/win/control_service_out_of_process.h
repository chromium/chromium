// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_CONTROL_SERVICE_OUT_OF_PROCESS_H_
#define CHROME_UPDATER_WIN_CONTROL_SERVICE_OUT_OF_PROCESS_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/control_service.h"
#include "chrome/updater/service_scope.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace updater {

// All functions and callbacks must be called on the same sequence.
class ControlServiceOutOfProcess : public ControlService {
 public:
  explicit ControlServiceOutOfProcess(ServiceScope scope);

  // Overrides for ControlService.
  void Run(base::OnceClosure callback) override;
  void InitializeUpdateService(base::OnceClosure callback) override;
  void Uninitialize() override;

 private:
  ~ControlServiceOutOfProcess() override;

  // These function are invoked on the |com_task_runner_|.
  void RunOnSTA(base::OnceClosure callback);
  void InitializeUpdateServiceOnSTA(base::OnceClosure callback);

  // Bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Runs the tasks which involve outbound COM calls and inbound COM callbacks.
  // This task runner is thread-affine with the COM STA.
  scoped_refptr<base::SingleThreadTaskRunner> com_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_CONTROL_SERVICE_OUT_OF_PROCESS_H_
