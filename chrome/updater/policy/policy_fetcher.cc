// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/policy_fetcher.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/enterprise_companion/constants.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion_client.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/dm_policy_manager.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/usage_stats_permissions.h"
#include "components/policy/core/common/policy_types.h"
#include "components/update_client/timed_callback.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/isolated_connection.h"

namespace updater {

using PolicyFetchCompleteCallback =
    base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>;

// `OutOfProcessPolicyFetcher` launches the enterprise companion app and
// delegates the policy fetch tasks to it through Mojom.
class OutOfProcessPolicyFetcher : public PolicyFetcher {
 public:
  OutOfProcessPolicyFetcher(scoped_refptr<PersistedData> persisted_data,
                            std::optional<bool> override_is_managed_device,
                            base::TimeDelta connection_timeout);

  // Overrides for `PolicyFetcher`.
  void FetchPolicies(policy::PolicyFetchReason reason,
                     PolicyFetchCompleteCallback callback) override;

 private:
  ~OutOfProcessPolicyFetcher() override;

  void OnConnected(
      policy::PolicyFetchReason reason,
      std::unique_ptr<mojo::IsolatedConnection> connection,
      mojo::Remote<enterprise_companion::mojom::EnterpriseCompanion> remote);
  void OnPoliciesFetched(enterprise_companion::mojom::StatusPtr status);

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<enterprise_companion::mojom::EnterpriseCompanion> remote_;
  std::unique_ptr<mojo::IsolatedConnection> connection_;
  PolicyFetchCompleteCallback fetch_complete_callback_;
  scoped_refptr<PersistedData> persisted_data_;
  const std::optional<bool> override_is_managed_device_;
  const base::TimeDelta connection_timeout_;
};

OutOfProcessPolicyFetcher::OutOfProcessPolicyFetcher(
    scoped_refptr<PersistedData> persisted_data,
    std::optional<bool> override_is_managed_device,
    base::TimeDelta connection_timeout)
    : persisted_data_(persisted_data),
      override_is_managed_device_(override_is_managed_device),
      connection_timeout_(connection_timeout) {}

OutOfProcessPolicyFetcher::~OutOfProcessPolicyFetcher() = default;

void OutOfProcessPolicyFetcher::FetchPolicies(
    policy::PolicyFetchReason reason,
    PolicyFetchCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  CHECK(!fetch_complete_callback_);
  fetch_complete_callback_ = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      update_client::MakeTimedCallback(std::move(callback), connection_timeout_,
                                       kErrorIpcDisconnect,
                                       scoped_refptr<PolicyManagerInterface>()),
      kErrorIpcDisconnect, nullptr);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce([] { return AnyAppEnablesUsageStats(GetUpdaterScope()); }),
      base::BindOnce(
          [](base::TimeDelta connection_timeout, const std::string& cohort_id,
             base::OnceCallback<void(
                 std::unique_ptr<mojo::IsolatedConnection>,
                 mojo::Remote<
                     enterprise_companion::mojom::EnterpriseCompanion>)>
                 callback,
             bool enable_usage_stats) {
            enterprise_companion::ConnectAndLaunchServer(
                base::DefaultClock::GetInstance(), connection_timeout,
                enable_usage_stats, cohort_id, std::move(callback));
          },
          connection_timeout_,
          persisted_data_->GetCohort(enterprise_companion::kCompanionAppId),
          base::BindOnce(&OutOfProcessPolicyFetcher::OnConnected,
                         base::WrapRefCounted(this), reason)));
}

void OutOfProcessPolicyFetcher::OnConnected(
    policy::PolicyFetchReason reason,
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
  remote_->FetchPolicies(
      reason, base::BindOnce(&OutOfProcessPolicyFetcher::OnPoliciesFetched,
                             base::WrapRefCounted(this)));
}

void OutOfProcessPolicyFetcher::OnPoliciesFetched(
    enterprise_companion::mojom::StatusPtr mojom_status) {
  if (!mojom_status) {
    VLOG(1) << "Received null status from out-of-process fetcher.";
    std::move(fetch_complete_callback_).Run(kErrorIpcDisconnect, nullptr);
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Policy fetch status: " << mojom_status->code
          << ", space: " << mojom_status->space
          << ", description: " << mojom_status->description;
  if (mojom_status->space == enterprise_companion::kStatusOk) {
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
    int result = kErrorPolicyFetchFailed;
    if (mojom_status->space == enterprise_companion::kStatusApplicationError &&
        mojom_status->code ==
            static_cast<int>(enterprise_companion::ApplicationError::
                                 kRegistrationPreconditionFailed)) {
      scoped_refptr<device_management_storage::DMStorage> dm_storage =
          device_management_storage::GetDefaultDMStorage();
      result = (dm_storage && dm_storage->IsEnrollmentMandatory())
                   ? kErrorDMRegistrationFailed
                   : kErrorOk;
    }
    std::move(fetch_complete_callback_).Run(result, nullptr);
  }
}

scoped_refptr<PolicyFetcher> CreateOutOfProcessPolicyFetcher(
    scoped_refptr<PersistedData> persisted_data,
    std::optional<bool> override_is_managed_device,
    base::TimeDelta override_ceca_connection_timeout) {
  return base::MakeRefCounted<OutOfProcessPolicyFetcher>(
      persisted_data, std::move(override_is_managed_device),
      override_ceca_connection_timeout);
}

}  // namespace updater
