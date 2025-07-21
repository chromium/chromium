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

namespace device_signals {

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

  const raw_ptr<policy::CloudPolicyManager> device_cloud_policy_manager_;
  base::WeakPtrFactory<AndroidOsSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_ANDROID_ANDROID_OS_SIGNALS_COLLECTOR_H_
