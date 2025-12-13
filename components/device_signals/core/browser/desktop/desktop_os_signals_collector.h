// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_DESKTOP_DESKTOP_OS_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_DESKTOP_DESKTOP_OS_SIGNALS_COLLECTOR_H_

#include "base/memory/weak_ptr.h"
#include "base/system/sys_info.h"
#include "components/device_signals/core/browser/base_signals_collector.h"

namespace policy {
class CloudPolicyManager;
}  // namespace policy

namespace device_signals {

struct OsSignalsResponse;

class DesktopOsSignalsCollector : public BaseSignalsCollector {
 public:
  explicit DesktopOsSignalsCollector(
      policy::CloudPolicyManager* device_cloud_policy_manager);

  ~DesktopOsSignalsCollector() override;

  DesktopOsSignalsCollector(const DesktopOsSignalsCollector&) = delete;
  DesktopOsSignalsCollector& operator=(const DesktopOsSignalsCollector&) =
      delete;

 private:
  void GetOsSignals(UserPermission permission,
                    const SignalsAggregationRequest& request,
                    SignalsAggregationResponse& response,
                    base::OnceClosure done_closure);

  void OnHardwareInfoRetrieved(
      UserPermission permission,
      const SignalsAggregationRequest& request,
      SignalsAggregationResponse& response,
      std::unique_ptr<OsSignalsResponse> os_signals_response,
      base::OnceClosure done_closure,
      base::SysInfo::HardwareInfo hardware_info);

  const raw_ptr<policy::CloudPolicyManager> device_cloud_policy_manager_;
  base::WeakPtrFactory<DesktopOsSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_DESKTOP_DESKTOP_OS_SIGNALS_COLLECTOR_H_
