// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_fetcher.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
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
#include "chrome/updater/util/util.h"
#include "url/gurl.h"

namespace updater {

PolicyFetcher::PolicyFetcher(
    const GURL& server_url,
    const std::optional<PolicyServiceProxyConfiguration>& proxy_configuration,
    const std::optional<bool>& override_is_managed_device)
    : server_url_(server_url),
      policy_service_proxy_configuration_(proxy_configuration),
      override_is_managed_device_(override_is_managed_device),
      sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
  VLOG(0) << "Policy server: " << server_url_.possibly_invalid_spec();
}

PolicyFetcher::~PolicyFetcher() = default;

void PolicyFetcher::FetchPolicies(
    base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>
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
  if (!dm_storage) {
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), false,
                       DMClient::RequestResult::kNoDefaultDMStorage));
    return;
  }
  VLOG(1) << "Enrollment token: " << dm_storage->GetEnrollmentToken();
  DMClient::RegisterDevice(
      DMClient::CreateDefaultConfigurator(server_url_,
                                          policy_service_proxy_configuration_),
      dm_storage,
      base::BindPostTask(main_task_runner,
                         base::BindOnce(std::move(callback),
                                        dm_storage->IsEnrollmentMandatory())));
}

void PolicyFetcher::OnRegisterDeviceRequestComplete(
    base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>
        callback,
    bool is_enrollment_mandatory,
    DMClient::RequestResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  if (result == DMClient::RequestResult::kSuccess ||
      result == DMClient::RequestResult::kAlreadyRegistered) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PolicyFetcher::FetchPolicy, this,
                       base::BindPostTaskToCurrentDefault(
                           base::BindOnce(std::move(callback), kErrorOk))));
  } else {
    VLOG(1) << "Device registration failed, skip fetching policies.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            is_enrollment_mandatory ? kErrorDMRegistrationFailed : kErrorOk,
            nullptr));
  }
}

void PolicyFetcher::FetchPolicy(
    base::OnceCallback<void(scoped_refptr<PolicyManagerInterface>)> callback) {
  VLOG(1) << __func__;

  DMClient::FetchPolicy(
      DMClient::CreateDefaultConfigurator(server_url_,
                                          policy_service_proxy_configuration_),
      GetDefaultDMStorage(),
      base::BindOnce(&PolicyFetcher::OnFetchPolicyRequestComplete, this)
          .Then(std::move(callback)));
}

scoped_refptr<PolicyManagerInterface>
PolicyFetcher::OnFetchPolicyRequestComplete(
    DMClient::RequestResult result,
    const std::vector<PolicyValidationResult>& validation_results) {
  VLOG(1) << __func__;

  if (result == DMClient::RequestResult::kSuccess)
    return CreateDMPolicyManager(override_is_managed_device_);

  for (const auto& validation_result : validation_results) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DMClient::ReportPolicyValidationErrors,
            DMClient::CreateDefaultConfigurator(
                server_url_, policy_service_proxy_configuration_),
            GetDefaultDMStorage(), validation_result,
            base::BindOnce([](DMClient::RequestResult result) {
              if (result != DMClient::RequestResult::kSuccess)
                LOG(WARNING)
                    << "DMClient::ReportPolicyValidationErrors failed: "
                    << result;
            })));
  }

  return nullptr;
}

}  // namespace updater
