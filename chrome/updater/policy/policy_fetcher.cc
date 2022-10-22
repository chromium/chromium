// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_fetcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_client.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/device_management/dm_storage.h"
#include "chrome/updater/policy/dm_policy_manager.h"
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

PolicyFetcher::PolicyFetcher(scoped_refptr<PolicyService> policy_service)
    : policy_service_(policy_service),
      policy_service_proxy_configuration_(
          PolicyServiceProxyConfiguration::Get(policy_service)),
      sequenced_task_runner_(GetBlockingTaskRunner()) {}

PolicyFetcher::~PolicyFetcher() = default;

void PolicyFetcher::FetchPolicies(
    base::OnceCallback<void(int, std::unique_ptr<PolicyManagerInterface>)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PolicyFetcher::RegisterDevice, this,
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&PolicyFetcher::OnRegisterDeviceRequestComplete, this,
                         std::move(callback))));
}

void PolicyFetcher::RegisterDevice(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    base::OnceCallback<void(bool, DMClient::RequestResult)> callback) {
  VLOG(1) << __func__;

  scoped_refptr<DMStorage> dm_storage = GetDefaultDMStorage();
  DMClient::RegisterDevice(
      DMClient::CreateDefaultConfigurator(policy_service_proxy_configuration_),
      dm_storage,
      base::BindPostTask(main_task_runner,
                         base::BindOnce(std::move(callback),
                                        dm_storage->IsEnrollmentMandatory())));
}

void PolicyFetcher::OnRegisterDeviceRequestComplete(
    base::OnceCallback<void(int, std::unique_ptr<PolicyManagerInterface>)>
        callback,
    bool is_enrollment_mandatory,
    DMClient::RequestResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  if (result == DMClient::RequestResult::kSuccess ||
      result == DMClient::RequestResult::kAlreadyRegistered) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PolicyFetcher::FetchPolicy, this,
            base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                               base::BindOnce(std::move(callback), kErrorOk))));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            is_enrollment_mandatory ? kErrorDMRegistrationFailed : kErrorOk,
            nullptr));
  }
}

void PolicyFetcher::FetchPolicy(
    base::OnceCallback<void(std::unique_ptr<PolicyManagerInterface>)>
        callback) {
  VLOG(1) << __func__;

  DMClient::FetchPolicy(
      DMClient::CreateDefaultConfigurator(policy_service_proxy_configuration_),
      GetDefaultDMStorage(),
      base::BindOnce(&PolicyFetcher::OnFetchPolicyRequestComplete, this)
          .Then(std::move(callback)));
}

std::unique_ptr<PolicyManagerInterface>
PolicyFetcher::OnFetchPolicyRequestComplete(
    DMClient::RequestResult result,
    const std::vector<PolicyValidationResult>& validation_results) {
  VLOG(1) << __func__;

  if (result == DMClient::RequestResult::kSuccess)
    return CreateDMPolicyManager();

  for (const auto& validation_result : validation_results) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DMClient::ReportPolicyValidationErrors,
            DMClient::CreateDefaultConfigurator(
                policy_service_proxy_configuration_),
            GetDefaultDMStorage(), validation_result,
            base::BindOnce([](DMClient::RequestResult result) {
              if (result != DMClient::RequestResult::kSuccess)
                LOG(WARNING)
                    << "DMClient::ReportPolicyValidationErrors failed: "
                    << static_cast<int>(result);
            })));
  }

  return nullptr;
}

}  // namespace updater
