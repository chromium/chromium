// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_SERVICE_IMPL_H_
#define CHROME_UPDATER_UPDATE_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/update_service.h"

namespace base {
class SequencedTaskRunner;
class Version;
}  // namespace base

namespace update_client {
class Configurator;
class UpdateClient;
}  // namespace update_client

namespace updater {
class CheckForUpdatesTask;
class PersistedData;
struct RegistrationRequest;
struct RegistrationResponse;

// All functions and callbacks must be called on the same sequence.
class UpdateServiceImpl : public UpdateService {
 public:
  explicit UpdateServiceImpl(scoped_refptr<update_client::Configurator> config);

  // Overrides for updater::UpdateService.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) const override;
  void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(const RegistrationResponse&)> callback) override;
  void RunPeriodicTasks(base::OnceClosure callback) override;
  void UpdateAll(StateChangeCallback state_update, Callback callback) override;
  void Update(const std::string& app_id,
              Priority priority,
              StateChangeCallback state_update,
              Callback callback) override;

  void Uninitialize() override;

 private:
  ~UpdateServiceImpl() override;

  // Runs the task at the head of `tasks_`, if any.
  void TaskStart();

  // Run `callback`, pops `tasks_`, and calls TaskStart.
  void TaskDone(base::OnceClosure callback);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<update_client::Configurator> config_;
  scoped_refptr<PersistedData> persisted_data_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<update_client::UpdateClient> update_client_;

  // The queue prevents multiple Task instances from running simultaneously and
  // processes them sequentially.
  base::queue<scoped_refptr<CheckForUpdatesTask>> tasks_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_SERVICE_IMPL_H_
