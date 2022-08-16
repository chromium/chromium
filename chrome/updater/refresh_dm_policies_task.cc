// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/refresh_dm_policies_task.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
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

RefreshDMPoliciesTask::RefreshDMPoliciesTask(
    scoped_refptr<Configurator> config,
    scoped_refptr<base::SequencedTaskRunner> main_task_runner)
    : config_(config),
      main_task_runner_(main_task_runner),
      sequenced_task_runner_(GetBlockingTaskRunner()) {}

RefreshDMPoliciesTask::~RefreshDMPoliciesTask() = default;

void RefreshDMPoliciesTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  // `RefreshDMPoliciesTask::FetchPolicy` can block and therefore is running
  // under a task runner with `base::MayBlock()`.
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RefreshDMPoliciesTask::FetchPolicy, this,
                                std::move(callback)));
}

void RefreshDMPoliciesTask::FetchPolicy(base::OnceClosure callback) {
  VLOG(1) << __func__;

  DMClient::FetchPolicy(
      DMClient::CreateDefaultConfigurator(config_->GetPolicyService()),
      GetDefaultDMStorage(),
      base::BindPostTask(
          main_task_runner_,
          base::BindOnce(&RefreshDMPoliciesTask::OnRequestComplete, this)
              .Then(std::move(callback))));
}

void RefreshDMPoliciesTask::OnRequestComplete(
    DMClient::RequestResult result,
    const std::vector<PolicyValidationResult>& validation_results) {
  VLOG(1) << __func__;

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
