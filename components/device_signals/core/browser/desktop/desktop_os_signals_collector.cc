// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/desktop/desktop_os_signals_collector.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/version_info/version_info.h"

namespace device_signals {

namespace {

std::unique_ptr<OsSignalsResponse> AddAsyncOsSignals(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    std::unique_ptr<OsSignalsResponse> os_signals_response) {
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    // PII signals requires user consent
    if (permission == UserPermission::kGranted) {
      os_signals_response->mac_addresses = device_signals::GetMacAddresses();
      os_signals_response->serial_number = device_signals::GetSerialNumber();
      os_signals_response->system_dns_servers =
          device_signals::GetSystemDnsServers();
    }

    os_signals_response->disk_encryption = device_signals::GetDiskEncrypted();
    os_signals_response->os_firewall = device_signals::GetOSFirewall();

#if BUILDFLAG(IS_LINUX)
    os_signals_response->distribution_version =
        device_signals::GetDistributionVersion();
#endif  // BUILDFLAG(IS_LINUX)
  }

  return os_signals_response;
}

void OnSignalsCollected(
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    std::unique_ptr<OsSignalsResponse> os_signals_response) {
  if (os_signals_response) {
    response.os_signals_response = std::move(*os_signals_response);
  }

  std::move(done_closure).Run();
}

}  // namespace

DesktopOsSignalsCollector::DesktopOsSignalsCollector(
    policy::CloudPolicyManager* device_cloud_policy_manager)
    : BaseSignalsCollector({
          {SignalName::kOsSignals,
           base::BindRepeating(&DesktopOsSignalsCollector::GetOsSignals,
                               base::Unretained(this))},
      }),
      device_cloud_policy_manager_(device_cloud_policy_manager) {}

DesktopOsSignalsCollector::~DesktopOsSignalsCollector() = default;

void DesktopOsSignalsCollector::GetOsSignals(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  if (permission != UserPermission::kGranted &&
      permission != UserPermission::kMissingConsent) {
    std::move(done_closure).Run();
    return;
  }

  auto signal_response = std::make_unique<OsSignalsResponse>();
  signal_response->operating_system = policy::GetOSPlatform();
  signal_response->os_version = device_signals::GetOsVersion();
  signal_response->browser_version = version_info::GetVersionNumber();
  signal_response->screen_lock_secured = device_signals::GetScreenlockSecured();
#if BUILDFLAG(IS_WIN)
  signal_response->secure_boot_mode = device_signals::GetSecureBootEnabled();
  signal_response->windows_machine_domain =
      device_signals::GetWindowsMachineDomain();
  signal_response->windows_user_domain = device_signals::GetWindowsUserDomain();
#endif  // BUILDFLAG(IS_WIN)

  // PII signals requires user consent
  if (permission == UserPermission::kGranted) {
    signal_response->display_name = policy::GetDeviceName();
    signal_response->hostname = device_signals::GetHostName();
#if BUILDFLAG(IS_WIN)
    signal_response->machine_guid = device_signals::GetMachineGuid();
#endif  // BUILDFLAG(IS_WIN)
  }

  signal_response->device_enrollment_domain =
      device_signals::TryGetEnrollmentDomain(device_cloud_policy_manager_);

  base::SysInfo::GetHardwareInfo(base::BindOnce(
      &DesktopOsSignalsCollector::OnHardwareInfoRetrieved,
      weak_factory_.GetWeakPtr(), permission, request, std::ref(response),
      std::move(signal_response), std::move(done_closure)));
}

void DesktopOsSignalsCollector::OnHardwareInfoRetrieved(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    std::unique_ptr<OsSignalsResponse> os_signals_response,
    base::OnceClosure done_closure,
    base::SysInfo::HardwareInfo hardware_info) {
  os_signals_response->device_manufacturer = hardware_info.manufacturer;
  os_signals_response->device_model = hardware_info.model;

  auto add_async_os_signals_callback = base::BindOnce(
      &AddAsyncOsSignals, permission, request, std::move(os_signals_response));
  auto on_signals_collected_callback = base::BindOnce(
      &OnSignalsCollected, std::ref(response), std::move(done_closure));

#if BUILDFLAG(IS_WIN)
  base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})
      ->PostTaskAndReplyWithResult(FROM_HERE,
                                   std::move(add_async_os_signals_callback),
                                   std::move(on_signals_collected_callback));
#else
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, std::move(add_async_os_signals_callback),
      std::move(on_signals_collected_callback));
#endif
}

}  // namespace device_signals
