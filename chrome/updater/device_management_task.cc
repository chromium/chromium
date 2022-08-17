// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/device_management_task.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/device_management/dm_client.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/policy/service.h"

namespace updater {

namespace {

scoped_refptr<base::SequencedTaskRunner> GetBlockingTaskRunner() {
  constexpr base::TaskTraits KMayBlockTraits = {base::MayBlock()};

#if BUILDFLAG(IS_WIN)
  return base::ThreadPool::CreateCOMSTATaskRunner(KMayBlockTraits);
#else
  return base::ThreadPool::CreateSequencedTaskRunner(KMayBlockTraits);
#endif
}

}  // namespace

DeviceManagementTask::DeviceManagementTask(
    scoped_refptr<Configurator> config,
    scoped_refptr<base::SequencedTaskRunner> main_task_runner)
    : config_(config),
      main_task_runner_(main_task_runner),
      sequenced_task_runner_(GetBlockingTaskRunner()) {}

DeviceManagementTask::~DeviceManagementTask() = default;

void DeviceManagementTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  // This call can block and therefore is running under a task runner with
  // `base::MayBlock()`.
  sequenced_task_runner_->PostTask(FROM_HERE, std::move(callback));
}

void DeviceManagementTask::RunRegisterDevice(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  Run(base::BindOnce(&DeviceManagementTask::RegisterDevice, this,
                     std::move(callback)));
}

void DeviceManagementTask::RegisterDevice(base::OnceClosure callback) {
  VLOG(1) << __func__;

  CallDMFunction(DMClient::RegisterDevice,
                 &DeviceManagementTask::OnRegisterDeviceRequestComplete,
                 std::move(callback));
}

void DeviceManagementTask::OnRegisterDeviceRequestComplete(
    DMClient::RequestResult result) {
  VLOG(1) << __func__;

  // TODO(crbug.com/1345407) : handle error cases when enrollment is mandatory.
  result_ = result;
}

void DeviceManagementTask::RunFetchPolicy(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  Run(base::BindOnce(&DeviceManagementTask::FetchPolicy, this,
                     std::move(callback)));
}

void DeviceManagementTask::FetchPolicy(base::OnceClosure callback) {
  VLOG(1) << __func__;

  CallDMFunction(DMClient::FetchPolicy,
                 &DeviceManagementTask::OnFetchPolicyRequestComplete,
                 std::move(callback));
}

void DeviceManagementTask::OnFetchPolicyRequestComplete(
    DMClient::RequestResult result,
    const std::vector<PolicyValidationResult>& validation_results) {
  VLOG(1) << __func__;

  result_ = result;
  if (result != DMClient::RequestResult::kSuccess) {
    for (const auto& validation_result : validation_results) {
      sequenced_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &DMClient::ReportPolicyValidationErrors,
              DMClient::CreateDefaultConfigurator(config_->GetPolicyService()),
              GetDefaultDMStorage(), validation_result,
              base::BindOnce([](DMClient::RequestResult result) {
                if (result != DMClient::RequestResult::kSuccess)
                  LOG(WARNING)
                      << "DMClient::ReportPolicyValidationErrors failed: "
                      << static_cast<int>(result);
              })));
    }

    return;
  }

  config_->ResetPolicyService();
  VLOG(1) << "Policies are now reloaded.";
}

}  // namespace updater
