// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UPDATE_SERVICE_INTERNAL_PROXY_H_
#define CHROME_UPDATER_WIN_UPDATE_SERVICE_INTERNAL_PROXY_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace updater {

// All functions and callbacks must be called on the same sequence.
class UpdateServiceInternalProxy : public UpdateServiceInternal {
 public:
  explicit UpdateServiceInternalProxy(UpdaterScope scope);

  // Overrides for UpdateServiceInternal.
  void Run(base::OnceClosure callback) override;
  void InitializeUpdateService(base::OnceClosure callback) override;
  void Uninitialize() override;

 private:
  ~UpdateServiceInternalProxy() override;

  // These function are invoked on the |com_task_runner_|.
  void RunOnSTA(base::OnceClosure callback);
  void InitializeUpdateServiceOnSTA(base::OnceClosure callback);

  // Bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Runs the tasks which involve outbound COM calls and inbound COM callbacks.
  // This task runner is thread-affine with the COM STA.
  scoped_refptr<base::SingleThreadTaskRunner> STA_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UPDATE_SERVICE_INTERNAL_PROXY_H_
