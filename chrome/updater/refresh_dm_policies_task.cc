// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/refresh_dm_policies_task.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/device_management/dm_client.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/policy/service.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>
#endif  // BUILDFLAG(IS_WIN)

namespace updater {

RefreshDMPoliciesTask::RefreshDMPoliciesTask(scoped_refptr<Configurator> config)
    : config_(config) {}

RefreshDMPoliciesTask::~RefreshDMPoliciesTask() = default;

void RefreshDMPoliciesTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

#if BUILDFLAG(IS_WIN)
  // Returning early, because we cannot write to the DM cache in Windows under
  // %ProgramFiles(x86)% without having administrative privileges.
  if (!::IsUserAnAdmin()) {
    std::move(callback).Run();
    return;
  }

  FetchPolicy();
  std::move(callback).Run();
  return;

#else   // BUILDFLAG(IS_WIN)

  // `RefreshDMPoliciesTask::FetchPolicy` can block and therefore is running
  // under a task runner with `base::MayBlock()`.
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&RefreshDMPoliciesTask::FetchPolicy, this),
          std::move(callback));
#endif  // BUILDFLAG(IS_WIN)
}

void RefreshDMPoliciesTask::FetchPolicy() {
  VLOG(1) << __func__;

  DMClient::FetchPolicy(
      DMClient::CreateDefaultConfigurator(config_->GetPolicyService()),
      GetDefaultDMStorage(),
      base::BindOnce(&RefreshDMPoliciesTask::OnRequestComplete, this));
}

void RefreshDMPoliciesTask::OnRequestComplete(
    DMClient::RequestResult result,
    const std::vector<PolicyValidationResult>& validation_results) {
  VLOG(1) << __func__;

  // TODO(crbug.com/1345407) : call ReportPolicyValidationErrors() when there's
  // an error.
  if (result != DMClient::RequestResult::kSuccess)
    return;

  config_->ResetPolicyService();
  VLOG(1) << "Policies are now reloaded.";
}

}  // namespace updater
