// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_DEVICE_MANAGEMENT_TASK_H_
#define CHROME_UPDATER_DEVICE_MANAGEMENT_TASK_H_

#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/device_management/dm_client.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/device_management/dm_storage.h"

namespace updater {

// The DeviceManagementTask handles registration and DM policy refreshes.
class DeviceManagementTask
    : public base::RefCountedThreadSafe<DeviceManagementTask> {
 public:
  DeviceManagementTask(
      scoped_refptr<Configurator> config,
      scoped_refptr<base::SequencedTaskRunner> main_task_runner);
  void RunRegisterDevice(base::OnceClosure callback);
  void RunFetchPolicy(base::OnceClosure callback);

  DMClient::RequestResult result() const { return result_; }

 private:
  friend class base::RefCountedThreadSafe<DeviceManagementTask>;
  virtual ~DeviceManagementTask();

  void Run(base::OnceClosure callback);

  void RegisterDevice(base::OnceClosure callback);
  void OnRegisterDeviceRequestComplete(DMClient::RequestResult result);

  void FetchPolicy(base::OnceClosure callback);
  void OnFetchPolicyRequestComplete(
      DMClient::RequestResult result,
      const std::vector<PolicyValidationResult>& validation_results);

  template <typename Function, typename Callback>
  void CallDMFunction(Function fn,
                      Callback member_callback,
                      base::OnceClosure callback) {
    fn(DMClient::CreateDefaultConfigurator(config_->GetPolicyService()),
       GetDefaultDMStorage(),
       base::BindPostTask(
           main_task_runner_,
           base::BindOnce(member_callback, this).Then(std::move(callback))));
  }

  SEQUENCE_CHECKER(sequence_checker_);
  const scoped_refptr<Configurator> config_;
  const scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  DMClient::RequestResult result_ = DMClient::RequestResult::kSuccess;
};

}  // namespace updater

#endif  // CHROME_UPDATER_DEVICE_MANAGEMENT_TASK_H_
