// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UPDATE_SERVICE_INTERNAL_PROXY_H_
#define CHROME_UPDATER_WIN_UPDATE_SERVICE_INTERNAL_PROXY_H_

#include <wrl/client.h>

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/server/win/updater_internal_idl.h"
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

  // These functions run on the `com_task_runner_`.
  HRESULT InitializeSTA();
  void UninitializeOnSTA();

  // These functions run on the `com_task_runner_`. `prev_hr` contains the
  // result of the previous callback invocation in a `Then` chain.
  void RunOnSTA(base::OnceClosure callback, HRESULT prev_hr);
  void InitializeUpdateServiceOnSTA(base::OnceClosure callback,
                                    HRESULT prev_hr);

  // Bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_main_);

  UpdaterScope scope_;

  // Bound to the main sequence.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Runs the tasks which involve outbound COM calls and inbound COM callbacks.
  // This task runner is thread-affine with the COM STA.
  scoped_refptr<base::SingleThreadTaskRunner> com_task_runner_;

  // IUpdaterInternal COM server instance owned by the STA. That means the
  // instance must be created and destroyed on the com_task_runner_.
  Microsoft::WRL::ComPtr<IUpdaterInternal> updater_internal_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UPDATE_SERVICE_INTERNAL_PROXY_H_
