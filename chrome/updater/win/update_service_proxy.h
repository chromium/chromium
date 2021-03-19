// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_UPDATE_SERVICE_PROXY_H_
#define CHROME_UPDATER_WIN_UPDATE_SERVICE_PROXY_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/server/win/updater_idl.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
class Version;
}  // namespace base

namespace update_client {
enum class Error;
}  // namespace update_client

namespace updater {

struct RegistrationRequest;
struct RegistrationResponse;

// There are two threads running the code in this module. The main sequence is
// bound to one thread, all the COM calls, inbound and outbound occur on the
// second thread which serializes the tasks and the invocations originating
// in the COM RPC runtime, which arrive sequentially but they are not sequenced
// through the task runner.

// All public functions and callbacks must be called on the same sequence.
class UpdateServiceProxy : public UpdateService {
 public:
  explicit UpdateServiceProxy(UpdaterScope updater_scope);

  // Overrides for updater::UpdateService.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) const override;
  void RegisterApp(const RegistrationRequest& request,
                   RegisterAppCallback callback) override;
  void RunPeriodicTasks(base::OnceClosure callback) override;
  void UpdateAll(StateChangeCallback state_update, Callback callback) override;
  void Update(const std::string& app_id,
              Priority priority,
              StateChangeCallback state_update,
              Callback callback) override;
  void Uninitialize() override;

 private:
  ~UpdateServiceProxy() override;

  // These functions runs on the |com_task_runner_|.
  void GetVersionOnSTA(
      base::OnceCallback<void(const base::Version&)> callback) const;
  void RegisterAppOnSTA(const RegistrationRequest& request,
                        RegisterAppCallback callback);
  void RunPeriodicTasksOnSTA(base::OnceClosure callback);
  void UpdateAllOnSTA(StateChangeCallback state_update, Callback callback);
  void UpdateOnSTA(const std::string& app_id,
                   StateChangeCallback state_update,
                   Callback callback);

  // Bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Bound to the main sequence.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // Runs the tasks which involve outbound COM calls and inbound COM callbacks.
  // This task runner is thread-affine with the COM STA.
  scoped_refptr<base::SingleThreadTaskRunner> com_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_UPDATE_SERVICE_PROXY_H_
