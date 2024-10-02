// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_fetcher.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/launch.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/device_management/dm_client.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/policy/dm_policy_manager.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "url/gurl.h"

namespace updater {

namespace {

void InstallCompanionAppIfNeeded() {
  std::optional<base::FilePath> companion_exe =
      GetBundledEnterpriseCompanionExecutablePath(GetUpdaterScope());
  if (!companion_exe) {
    return;
  }
  base::CommandLine command_line(*companion_exe);
  command_line.AppendSwitch(enterprise_companion::kInstallIfNeededSwitch);
  int exit_code = -1;
  base::Process process = base::LaunchProcess(command_line, {});
  if (!process.IsValid()) {
    VLOG(1) << "Failed to launch " << command_line.GetCommandLineString();
    return;
  }
  if (!process.WaitForExitWithTimeout(base::Minutes(5), &exit_code)) {
    VLOG(1) << "Timed out waiting for " << command_line.GetCommandLineString();
    process.Terminate(1, false);
    return;
  }
  VLOG(1) << __func__ << ": " << command_line.GetCommandLineString()
          << " exited with code " << exit_code;
}

}  // namespace

using PolicyFetchCompleteCallback =
    base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>;

FallbackPolicyFetcher::FallbackPolicyFetcher(scoped_refptr<PolicyFetcher> impl,
                                             scoped_refptr<PolicyFetcher> next)
    : impl_(impl), next_(next) {}

FallbackPolicyFetcher::~FallbackPolicyFetcher() = default;

void FallbackPolicyFetcher::FetchPolicies(
    PolicyFetchCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  fetch_complete_callback_ = std::move(callback);
  impl_->FetchPolicies(base::BindOnce(&FallbackPolicyFetcher::PolicyFetched,
                                      base::Unretained(this)));
}

void FallbackPolicyFetcher::PolicyFetched(
    int result,
    scoped_refptr<PolicyManagerInterface> policy_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__ << " result: " << result;
  if (result != 0 && next_) {
    VLOG(1) << __func__ << ": Falling back to next PolicyFetcher.";
    next_->FetchPolicies(std::move(fetch_complete_callback_));
  } else {
    std::move(fetch_complete_callback_).Run(result, policy_manager);
  }
}

class InProcessPolicyFetcher : public PolicyFetcher {
 public:
  InProcessPolicyFetcher(
      const GURL& server_url,
      std::optional<PolicyServiceProxyConfiguration> proxy_configuration,
      std::optional<bool> override_is_managed_device);
  void FetchPolicies(PolicyFetchCompleteCallback callback) override;

 private:
  ~InProcessPolicyFetcher() override;

  void RegisterDevice(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      base::OnceCallback<void(bool, DMClient::RequestResult)> callback);
  void OnRegisterDeviceRequestComplete(PolicyFetchCompleteCallback callback,
                                       bool is_enrollment_mandatory,
                                       DMClient::RequestResult result);

  void FetchPolicy(
      base::OnceCallback<void(scoped_refptr<PolicyManagerInterface>)> callback);
  scoped_refptr<PolicyManagerInterface> OnFetchPolicyRequestComplete(
      DMClient::RequestResult result,
      const std::vector<PolicyValidationResult>& validation_results);

  SEQUENCE_CHECKER(sequence_checker_);
  const GURL server_url_;
  const std::optional<PolicyServiceProxyConfiguration>
      policy_service_proxy_configuration_;
  const std::optional<bool> override_is_managed_device_;
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

InProcessPolicyFetcher::InProcessPolicyFetcher(
    const GURL& server_url,
    std::optional<PolicyServiceProxyConfiguration> proxy_configuration,
    std::optional<bool> override_is_managed_device)
    : server_url_(server_url),
      policy_service_proxy_configuration_(std::move(proxy_configuration)),
      override_is_managed_device_(std::move(override_is_managed_device)),
      sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
  VLOG(0) << "Policy server: " << server_url_.possibly_invalid_spec();
}

InProcessPolicyFetcher::~InProcessPolicyFetcher() = default;

void InProcessPolicyFetcher::FetchPolicies(
    PolicyFetchCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InProcessPolicyFetcher::RegisterDevice, base::Unretained(this),
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(
              &InProcessPolicyFetcher::OnRegisterDeviceRequestComplete,
              base::Unretained(this), std::move(callback))));
}

void InProcessPolicyFetcher::RegisterDevice(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    base::OnceCallback<void(bool, DMClient::RequestResult)> callback) {
  VLOG(1) << __func__;
  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
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

void InProcessPolicyFetcher::OnRegisterDeviceRequestComplete(
    PolicyFetchCompleteCallback callback,
    bool is_enrollment_mandatory,
    DMClient::RequestResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;

  if (result == DMClient::RequestResult::kSuccess ||
      result == DMClient::RequestResult::kAlreadyRegistered) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InProcessPolicyFetcher::FetchPolicy,
                       base::Unretained(this),
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

void InProcessPolicyFetcher::FetchPolicy(
    base::OnceCallback<void(scoped_refptr<PolicyManagerInterface>)> callback) {
  VLOG(1) << __func__;
  DMClient::FetchPolicy(
      DMClient::CreateDefaultConfigurator(server_url_,
                                          policy_service_proxy_configuration_),
      device_management_storage::GetDefaultDMStorage(),
      base::BindOnce(&InProcessPolicyFetcher::OnFetchPolicyRequestComplete,
                     base::Unretained(this))
          .Then(std::move(callback)));
}

scoped_refptr<PolicyManagerInterface>
InProcessPolicyFetcher::OnFetchPolicyRequestComplete(
    DMClient::RequestResult result,
    const std::vector<PolicyValidationResult>& validation_results) {
  VLOG(1) << __func__;

  if (result == DMClient::RequestResult::kSuccess) {
    return CreateDMPolicyManager(override_is_managed_device_);
  }

  for (const auto& validation_result : validation_results) {
    VLOG(1) << "Sending policy validation error, status: "
            << validation_result.status
            << ", issues cout: " << validation_result.issues.size();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DMClient::ReportPolicyValidationErrors,
            DMClient::CreateDefaultConfigurator(
                server_url_, policy_service_proxy_configuration_),
            device_management_storage::GetDefaultDMStorage(), validation_result,
            base::BindOnce([](DMClient::RequestResult result) {
              if (result != DMClient::RequestResult::kSuccess) {
                LOG(WARNING)
                    << "DMClient::ReportPolicyValidationErrors failed: "
                    << result;
              }
            })));
  }

  return nullptr;
}

// `OutOfProcessPolicyFetcher` launches the enterprise companion app and
// delegates the policy fetch tasks to it through Mojom.
class OutOfProcessPolicyFetcher : public PolicyFetcher {
 public:
  OutOfProcessPolicyFetcher(bool usage_stats_enabled,
                            std::optional<bool> override_is_managed_device,
                            base::TimeDelta connection_timeout);

  // Overrides for `PolicyFetcher`.
  void FetchPolicies(PolicyFetchCompleteCallback callback) override;

 private:
  ~OutOfProcessPolicyFetcher() override;

  void OnInstallIfNeededComplete();
  void OnConnected(
      std::unique_ptr<mojo::IsolatedConnection> connection,
      mojo::Remote<enterprise_companion::mojom::EnterpriseCompanion> remote);
  void OnPoliciesFetched(enterprise_companion::mojom::StatusPtr status);
  void OnRPCDropped();

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<enterprise_companion::mojom::EnterpriseCompanion> remote_;
  std::unique_ptr<mojo::IsolatedConnection> connection_;
  PolicyFetchCompleteCallback fetch_complete_callback_;
  const bool usage_stats_enabled_;
  const std::optional<bool> override_is_managed_device_;
  const base::TimeDelta connection_timeout_;
};

OutOfProcessPolicyFetcher::OutOfProcessPolicyFetcher(
    bool usage_stats_enabled,
    std::optional<bool> override_is_managed_device,
    base::TimeDelta connection_timeout)
    : usage_stats_enabled_(usage_stats_enabled),
      override_is_managed_device_(override_is_managed_device),
      connection_timeout_(connection_timeout) {}

OutOfProcessPolicyFetcher::~OutOfProcessPolicyFetcher() = default;

void OutOfProcessPolicyFetcher::FetchPolicies(
    PolicyFetchCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  CHECK(!fetch_complete_callback_);
  fetch_complete_callback_ = std::move(callback);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(&InstallCompanionAppIfNeeded),
      base::BindOnce(&OutOfProcessPolicyFetcher::OnInstallIfNeededComplete,
                     base::WrapRefCounted(this)));
}

void OutOfProcessPolicyFetcher::OnInstallIfNeededComplete() {
  enterprise_companion::ConnectAndLaunchServer(
      base::DefaultClock::GetInstance(), connection_timeout_,
      usage_stats_enabled_,
      base::BindOnce(&OutOfProcessPolicyFetcher::OnConnected,
                     base::WrapRefCounted(this)));
}

void OutOfProcessPolicyFetcher::OnConnected(
    std::unique_ptr<mojo::IsolatedConnection> connection,
    mojo::Remote<enterprise_companion::mojom::EnterpriseCompanion> remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  if (!connection || !remote) {
    VLOG(1) << "Failed to establish IPC connection with the companion app for "
               "policy fetch.";
    std::move(fetch_complete_callback_)
        .Run(kErrorMojoConnectionFailure, nullptr);
    return;
  }

  connection_ = std::move(connection);
  remote_ = std::move(remote);
  remote_->FetchPolicies(mojo::WrapCallbackWithDropHandler(
      base::BindOnce(&OutOfProcessPolicyFetcher::OnPoliciesFetched,
                     base::WrapRefCounted(this)),
      base::BindOnce(&OutOfProcessPolicyFetcher::OnRPCDropped,
                     base::WrapRefCounted(this))));
}

void OutOfProcessPolicyFetcher::OnPoliciesFetched(
    enterprise_companion::mojom::StatusPtr status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Policy fetch status: " << status->code
          << ", space: " << status->space
          << ", description: " << status->description;
  if (status->code == 0 && status->space == 0) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
        base::BindOnce(
            [](std::optional<bool> override_is_managed_device) {
              return CreateDMPolicyManager(override_is_managed_device);
            },
            override_is_managed_device_),
        base::BindOnce(
            [](PolicyFetchCompleteCallback callback,
               scoped_refptr<PolicyManagerInterface> dm_policy_manager) {
              std::move(callback).Run(kErrorOk, dm_policy_manager);
            },
            std::move(fetch_complete_callback_)));
  } else {
    std::move(fetch_complete_callback_).Run(kErrorPolicyFetchFailed, nullptr);
  }
}

void OutOfProcessPolicyFetcher::OnRPCDropped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "RPC connection dropped during policy fetch.";
  std::move(fetch_complete_callback_).Run(kErrorIpcDisconnect, nullptr);
}

scoped_refptr<PolicyFetcher> CreateInProcessPolicyFetcher(
    const GURL& server_url,
    std::optional<PolicyServiceProxyConfiguration> proxy_configuration,
    std::optional<bool> override_is_managed_device) {
  return base::MakeRefCounted<InProcessPolicyFetcher>(
      server_url, std::move(proxy_configuration),
      std::move(override_is_managed_device));
}

scoped_refptr<PolicyFetcher> CreateOutOfProcessPolicyFetcher(
    bool usage_stats_enabled,
    std::optional<bool> override_is_managed_device,
    base::TimeDelta override_ceca_connection_timeout) {
  return base::MakeRefCounted<OutOfProcessPolicyFetcher>(
      usage_stats_enabled, std::move(override_is_managed_device),
      override_ceca_connection_timeout);
}

}  // namespace updater
