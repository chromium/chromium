// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_ANDROID_OS_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_ANDROID_OS_SIGNALS_COLLECTOR_H_

#include "base/memory/weak_ptr.h"
#include "components/device_signals/core/browser/base_signals_collector.h"

namespace policy {
class CloudPolicyManager;
}  // namespace policy

namespace safe_browsing {

enum class VerifyAppsEnabledResult;
enum class HasHarmfulAppsResultStatus;

}  // namespace safe_browsing

namespace device_signals {

struct OsSignalsResponse;

class AndroidOsSignalsCollector : public BaseSignalsCollector {
 public:
  explicit AndroidOsSignalsCollector(
      policy::CloudPolicyManager* device_cloud_policy_manager);

  ~AndroidOsSignalsCollector() override;

  AndroidOsSignalsCollector(const AndroidOsSignalsCollector&) = delete;
  AndroidOsSignalsCollector& operator=(const AndroidOsSignalsCollector&) =
      delete;

 private:
  void GetOsSignals(UserPermission permission,
                    const SignalsAggregationRequest& request,
                    SignalsAggregationResponse& response,
                    base::OnceClosure done_closure);

  void OnIsVerifyAppsEnabled(
      SignalsAggregationResponse& response,
      std::unique_ptr<OsSignalsResponse> os_signals_response,
      base::OnceClosure done_closure,
      safe_browsing::VerifyAppsEnabledResult result);

  void OnHasPotentiallyHarmfulApps(
      SignalsAggregationResponse& response,
      std::unique_ptr<OsSignalsResponse> os_signals_response,
      base::OnceClosure done_closure,
      safe_browsing::HasHarmfulAppsResultStatus result,
      int num_of_apps,
      int status_code);

  const raw_ptr<policy::CloudPolicyManager> device_cloud_policy_manager_;
  base::WeakPtrFactory<AndroidOsSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_ANDROID_OS_SIGNALS_COLLECTOR_H_
