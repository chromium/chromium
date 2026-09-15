// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/android/android_signals_collector.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/version_info/version_info.h"

using safe_browsing::HasHarmfulAppsResultStatus;
using safe_browsing::SafeBrowsingApiHandlerBridge;
using safe_browsing::VerifyAppsEnabledResult;

namespace device_signals {

namespace {

void LogSafetyNetMetrics(HasHarmfulAppsResultStatus result,
                         int num_of_apps,
                         base::TimeTicks start_time) {
  static constexpr char kHarmfulAppsResultHistogram[] =
      "Enterprise.DeviceSignals.HarmfulApps.%s";
  // TODO(crbug.com/449183636): Ideally we should log the reason of failure as
  // well.
  base::UmaHistogramEnumeration(
      base::StringPrintf(kHarmfulAppsResultHistogram, "Result"), result);
  if (result == HasHarmfulAppsResultStatus::SUCCESS) {
    base::UmaHistogramCounts100(
        base::StringPrintf(kHarmfulAppsResultHistogram, "Count"), num_of_apps);

    base::TimeDelta latency = base::TimeTicks::Now() - start_time;
    base::UmaHistogramTimes("Enterprise.DeviceSignals.SafetyNet.Latency",
                            latency);
  }
}

}  // namespace

AndroidSignalsCollector::AndroidSignalsCollector(
    policy::CloudPolicyManager* device_cloud_policy_manager)
    : BaseSignalsCollector({
          {SignalName::kOsSignals,
           base::BindRepeating(&AndroidSignalsCollector::GetOsSignals,
                               base::Unretained(this))},
          {SignalName::kVerifyApps,
           base::BindRepeating(&AndroidSignalsCollector::GetVerifyApps,
                               base::Unretained(this))},
      }),
      device_cloud_policy_manager_(device_cloud_policy_manager) {}

AndroidSignalsCollector::~AndroidSignalsCollector() = default;

void AndroidSignalsCollector::GetOsSignals(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  if (permission != UserPermission::kGranted &&
      permission != UserPermission::kMissingConsent) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(done_closure));
    return;
  }

  OsSignalsResponse signal_response;

  if (permission == UserPermission::kGranted) {
    // TODO(crbug.com/449189531): Refactor and use `policy::GetDeviceName()`
    // instead.
    signal_response.display_name = base::android::device_info::device_name();
  }
  signal_response.operating_system = policy::GetOSPlatform();
  signal_response.os_version = base::SysInfo::OperatingSystemVersion();
  signal_response.browser_version = version_info::GetVersionNumber();
  signal_response.device_model = base::android::android_info::model();
  signal_response.device_manufacturer =
      base::android::android_info::manufacturer();
  signal_response.device_enrollment_domain =
      device_signals::TryGetEnrollmentDomain(device_cloud_policy_manager_);
  signal_response.security_patch_ms =
      device_signals::GetSecurityPatchLevelEpoch();

  response.os_signals_response = std::move(signal_response);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(done_closure));
}

void AndroidSignalsCollector::GetVerifyApps(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  if (permission != UserPermission::kGranted &&
      permission != UserPermission::kMissingConsent) {
    std::move(done_closure).Run();
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  safe_browsing::SafeBrowsingApiHandlerBridge::GetInstance()
      .StartIsVerifyAppsEnabled(
          base::BindOnce(&AndroidSignalsCollector::OnIsVerifyAppsEnabled,
                         weak_factory_.GetWeakPtr(), std::ref(response),
                         std::move(done_closure), start_time));
}

void AndroidSignalsCollector::OnIsVerifyAppsEnabled(
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    base::TimeTicks start_time,
    VerifyAppsEnabledResult result) {
  VerifyAppsSignalsResponse verify_apps_response;
  verify_apps_response.verified_apps_enabled =
      (result == VerifyAppsEnabledResult::SUCCESS_ENABLED ||
       result == VerifyAppsEnabledResult::SUCCESS_ALREADY_ENABLED);

  safe_browsing::SafeBrowsingApiHandlerBridge::GetInstance()
      .StartHasPotentiallyHarmfulApps(
          base::BindOnce(&AndroidSignalsCollector::OnHasPotentiallyHarmfulApps,
                         weak_factory_.GetWeakPtr(), std::ref(response),
                         std::move(done_closure),
                         std::move(verify_apps_response), start_time));
}

void AndroidSignalsCollector::OnHasPotentiallyHarmfulApps(
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    VerifyAppsSignalsResponse verify_apps_response,
    base::TimeTicks start_time,
    HasHarmfulAppsResultStatus result,
    int num_of_apps,
    int status_code) {
  if (result != HasHarmfulAppsResultStatus::SUCCESS) {
    VLOG_POLICY(1, REPORTING)
        << "HasPotentiallyHarmfulApps failed with status "
        << static_cast<int>(result) << " and code " << status_code;
  }

  verify_apps_response.has_potentially_harmful_apps =
      result == HasHarmfulAppsResultStatus::SUCCESS && num_of_apps != 0;

  LogSafetyNetMetrics(result, num_of_apps, start_time);

  response.verify_apps_signals_response = std::move(verify_apps_response);
  std::move(done_closure).Run();
}

}  // namespace device_signals
