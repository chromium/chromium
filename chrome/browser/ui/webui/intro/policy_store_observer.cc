// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/policy_store_observer.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_identity.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// PolicyStoreState will make it easier to handle all the states in a single
// callback.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PolicyStoreState {
  // Store was already loaded when we attached the observer.
  kSuccessAlreadyLoaded = 0,
  // Store has been loaded before the time delay ends.
  kSuccess = 1,
  // Store did not load in time.
  kTimeout = 2,
  // OnStoreError called.
  kStoreError = 3,
  // Store is null for a managed device.
  kStoreNull = 4,
  kMaxValue = kStoreNull,
};

policy::CloudPolicyStore* GetCloudPolicyStore() {
  auto* machine_level_manager = g_browser_process->browser_policy_connector()
                                    ->machine_level_user_cloud_policy_manager();

  return machine_level_manager ? machine_level_manager->core()->store()
                               : nullptr;
}

void RecordDisclaimerMetrics(PolicyStoreState state,
                             base::TimeTicks start_time) {
  base::UmaHistogramEnumeration("ProfilePicker.FirstRun.PolicyStoreState",
                                state);
  if (state == PolicyStoreState::kSuccess) {
    base::UmaHistogramTimes(
        "ProfilePicker.FirstRun.OrganizationAvailableTiming",
        /*sample*/ base::TimeTicks::Now() - start_time);
  }
}

void HandlePolicyStoreStatusChange(
    PolicyStoreState state,
    base::TimeTicks start_time,
    base::OnceCallback<void(std::string)> handle_policy_store_change) {
  RecordDisclaimerMetrics(state, start_time);
  std::string managed_device_disclaimer;
  if (state == PolicyStoreState::kSuccess ||
      state == PolicyStoreState::kSuccessAlreadyLoaded) {
    std::optional<std::string> manager = GetDeviceManagerIdentity();
    managed_device_disclaimer =
        (!manager.has_value() || manager->empty())
            ? l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION)
            : l10n_util::GetStringFUTF8(IDS_FRE_MANAGED_BY_DESCRIPTION,
                                        base::UTF8ToUTF16(*manager));
  } else {
    managed_device_disclaimer =
        l10n_util::GetStringUTF8(IDS_FRE_MANAGED_DESCRIPTION);
  }
  std::move(handle_policy_store_change).Run(managed_device_disclaimer);
}

}  // namespace

PolicyStoreObserver::PolicyStoreObserver(
    base::OnceCallback<void(std::string)> handle_policy_store_change)
    : handle_policy_store_change_(std::move(handle_policy_store_change)) {
  CHECK(handle_policy_store_change_);
  start_time_ = base::TimeTicks::Now();

  // Update the disclaimer directly if the policy store is already loaded.
  auto* policy_store = GetCloudPolicyStore();

  // GetCloudPolicyStore will return nullptr for managed devices with
  // non-branded builds because the machine level cloud policy manager will be
  // null while the device is still managed. In that case, we show a generic
  // disclaimer.
  if (!policy_store) {
    // The device is not enrolled in Chrome Browser Cloud Management
    HandlePolicyStoreStatusChange(PolicyStoreState::kStoreNull, start_time_,
                                  std::move(handle_policy_store_change_));
    return;
  }

  if (policy_store->is_initialized()) {
    HandlePolicyStoreStatusChange(PolicyStoreState::kSuccessAlreadyLoaded,
                                  start_time_,
                                  std::move(handle_policy_store_change_));
    return;
  }

  policy_store_observation_.Observe(policy_store);
  // 2.5 is the chrome logo animation time which is 1.5s plus the maximum
  // delay of 1s that we are willing to wait for.
  constexpr auto kMaximumEnterpriseDisclaimerDelay = base::Seconds(2.5);
  on_organization_fetch_timeout_.Reset(
      base::BindOnce(&PolicyStoreObserver::OnOrganizationFetchTimeout,
                     base::Unretained(this)));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, on_organization_fetch_timeout_.callback(),
      kMaximumEnterpriseDisclaimerDelay);
}

PolicyStoreObserver::~PolicyStoreObserver() = default;

void PolicyStoreObserver::OnStoreLoaded(policy::CloudPolicyStore* store) {
  on_organization_fetch_timeout_.Cancel();
  policy_store_observation_.Reset();
  HandlePolicyStoreStatusChange(PolicyStoreState::kSuccess, start_time_,
                                std::move(handle_policy_store_change_));
}

void PolicyStoreObserver::OnStoreError(policy::CloudPolicyStore* store) {
  on_organization_fetch_timeout_.Cancel();
  policy_store_observation_.Reset();
  HandlePolicyStoreStatusChange(PolicyStoreState::kStoreError, start_time_,
                                std::move(handle_policy_store_change_));
}

void PolicyStoreObserver::OnOrganizationFetchTimeout() {
  policy_store_observation_.Reset();
  HandlePolicyStoreStatusChange(PolicyStoreState::kTimeout, start_time_,
                                std::move(handle_policy_store_change_));
}
