// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_ANDROID_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_ANDROID_SIGNALS_COLLECTOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/device_signals/core/browser/base_signals_collector.h"

namespace policy {
class CloudPolicyManager;
}  // namespace policy

namespace safe_browsing {

enum class VerifyAppsEnabledResult;
enum class HasHarmfulAppsResultStatus;

}  // namespace safe_browsing

namespace device_signals {

class OsSecurityInfoProvider;
struct VerifyAppsSignalsResponse;

class AndroidSignalsCollector : public BaseSignalsCollector {
 public:
  // Returns the affiliation IDs of the organization managing the browser (e.g.
  // via `ChromeBrowserPolicyConnector`, which `//components` cannot include).
  // Run on every collection rather than reading `device_cloud_policy_manager`,
  // since on Android that manager itself can be created after this collector.
  using DeviceAffiliationIdsCallback =
      base::RepeatingCallback<base::flat_set<std::string>()>;

  AndroidSignalsCollector(
      DeviceAffiliationIdsCallback device_affiliation_ids_callback,
      policy::CloudPolicyManager* device_cloud_policy_manager,
      std::unique_ptr<OsSecurityInfoProvider> os_security_info_provider);

  ~AndroidSignalsCollector() override;

  AndroidSignalsCollector(const AndroidSignalsCollector&) = delete;
  AndroidSignalsCollector& operator=(const AndroidSignalsCollector&) = delete;

 private:
  void GetOsSignals(UserPermission permission,
                    const SignalsAggregationRequest& request,
                    SignalsAggregationResponse& response,
                    base::OnceClosure done_closure);

  void GetVerifyApps(UserPermission permission,
                     const SignalsAggregationRequest& request,
                     SignalsAggregationResponse& response,
                     base::OnceClosure done_closure);

  void OnIsVerifyAppsEnabled(SignalsAggregationResponse& response,
                             base::OnceClosure done_closure,
                             base::TimeTicks start_time,
                             safe_browsing::VerifyAppsEnabledResult result);

  void OnHasPotentiallyHarmfulApps(
      SignalsAggregationResponse& response,
      base::OnceClosure done_closure,
      VerifyAppsSignalsResponse verify_apps_response,
      base::TimeTicks start_time,
      safe_browsing::HasHarmfulAppsResultStatus result,
      int num_of_apps,
      int status_code);

  const DeviceAffiliationIdsCallback device_affiliation_ids_callback_;
  const raw_ptr<policy::CloudPolicyManager> device_cloud_policy_manager_;
  const std::unique_ptr<OsSecurityInfoProvider> os_security_info_provider_;
  base::WeakPtrFactory<AndroidSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_ANDROID_SIGNALS_COLLECTOR_H_
