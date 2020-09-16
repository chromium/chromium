// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/constants/dbus_switches.h"

namespace {

const char kCrOSTracingAgentName[] = "cros";
const char kCrOSTraceLabel[] = "systemTraceEvents";

}  // namespace

namespace chromeos {

FakeDebugDaemonClient::FakeDebugDaemonClient()
    : features_mask_(DebugDaemonClient::DEV_FEATURE_NONE),
      service_is_available_(true) {}

FakeDebugDaemonClient::~FakeDebugDaemonClient() = default;

void FakeDebugDaemonClient::Init(dbus::Bus* bus) {}

void FakeDebugDaemonClient::DumpDebugLogs(bool is_compressed,
                                          int file_descriptor,
                                          VoidDBusMethodCallback callback) {
  std::move(callback).Run(true);
}

void FakeDebugDaemonClient::SetDebugMode(const std::string& subsystem,
                                         VoidDBusMethodCallback callback) {
  std::move(callback).Run(false);
}

void FakeDebugDaemonClient::SetKstaledRatio(uint8_t val,
                                            KstaledRatioCallback callback) {
  // We just return true.
  std::move(callback).Run(true /* success */);
}

void FakeDebugDaemonClient::SetSwapParameter(
    const std::string& parameter,
    int32_t value,
    DBusMethodCallback<std::string> callback) {
  std::move(callback).Run(std::string());
}

std::string FakeDebugDaemonClient::GetTracingAgentName() {
  return kCrOSTracingAgentName;
}

std::string FakeDebugDaemonClient::GetTraceEventLabel() {
  return kCrOSTraceLabel;
}

void FakeDebugDaemonClient::StartAgentTracing(
    const base::trace_event::TraceConfig& trace_config,
    StartAgentTracingCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetTracingAgentName(),
                                true /* success */));
}

void FakeDebugDaemonClient::StopAgentTracing(
    StopAgentTracingCallback callback) {
  std::string trace_data = "# tracer: nop\n";
  std::move(callback).Run(GetTracingAgentName(), GetTraceEventLabel(),
                          base::RefCountedString::TakeString(&trace_data));
}

void FakeDebugDaemonClient::SetStopAgentTracingTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner) {}

void FakeDebugDaemonClient::GetRoutes(
    bool numeric,
    bool ipv6,
    DBusMethodCallback<std::vector<std::string>> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
}

void FakeDebugDaemonClient::GetNetworkStatus(
    DBusMethodCallback<std::string> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
}

void FakeDebugDaemonClient::GetNetworkInterfaces(
    DBusMethodCallback<std::string> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
}

void FakeDebugDaemonClient::GetPerfOutput(
    base::TimeDelta duration,
    const std::vector<std::string>& perf_args,
    int file_descriptor,
    DBusMethodCallback<uint64_t> error_callback) {}

void FakeDebugDaemonClient::StopPerf(uint64_t session_id,
                                     VoidDBusMethodCallback callback) {}

void FakeDebugDaemonClient::GetScrubbedBigLogs(
    const cryptohome::AccountIdentifier& id,
    GetLogsCallback callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Scrubbed Big Log"] = "Your email address is xxxxxxxx";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false, sample));
}

void FakeDebugDaemonClient::BackupArcBugReport(
    const cryptohome::AccountIdentifier& id,
    VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::GetAllLogs(GetLogsCallback callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Log"] = "Your email address is abc@abc.com";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false, sample));
}

void FakeDebugDaemonClient::GetLog(const std::string& log_name,
                                   DBusMethodCallback<std::string> callback) {
  std::string result = log_name + ": response from GetLog";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void FakeDebugDaemonClient::TestICMP(const std::string& ip_address,
                                     TestICMPCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
}

void FakeDebugDaemonClient::TestICMPWithOptions(
    const std::string& ip_address,
    const std::map<std::string, std::string>& options,
    TestICMPCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
}

void FakeDebugDaemonClient::UploadCrashes(UploadCrashesCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::EnableDebuggingFeatures(
    const std::string& password,
    EnableDebuggingCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::QueryDebuggingFeatures(
    QueryDevFeaturesCallback callback) {
  bool supported = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kSystemDevMode);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), true,
          static_cast<int>(
              supported ? features_mask_
                        : debugd::DevFeatureFlag::DEV_FEATURES_DISABLED)));
}

void FakeDebugDaemonClient::RemoveRootfsVerification(
    EnableDebuggingCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  if (service_is_available_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeDebugDaemonClient::SetOomScoreAdj(
    const std::map<pid_t, int32_t>& pid_to_oom_score_adj,
    SetOomScoreAdjCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true, ""));
}

void FakeDebugDaemonClient::SetDebuggingFeaturesStatus(int features_mask) {
  features_mask_ = features_mask;
}

void FakeDebugDaemonClient::SetServiceIsAvailable(bool is_available) {
  service_is_available_ = is_available;
  if (!is_available)
    return;

  std::vector<WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(true);
}

void FakeDebugDaemonClient::CupsAddManuallyConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    const std::string& ppd_contents,
    CupsAddPrinterCallback callback) {
  printers_.insert(name);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0));
}

void FakeDebugDaemonClient::CupsAddAutoConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    CupsAddPrinterCallback callback) {
  printers_.insert(name);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0));
}

void FakeDebugDaemonClient::CupsRemovePrinter(
    const std::string& name,
    CupsRemovePrinterCallback callback,
    base::OnceClosure error_callback) {
  const bool has_printer = base::Contains(printers_, name);
  if (has_printer)
    printers_.erase(name);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), has_printer));
}

void FakeDebugDaemonClient::StartPluginVmDispatcher(
    const std::string& /* owner_id */,
    const std::string& /* lang */,
    PluginVmDispatcherCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::StopPluginVmDispatcher(
    PluginVmDispatcherCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::SetRlzPingSent(SetRlzPingSentCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::SetSchedulerConfigurationV2(
    const std::string& config_name,
    bool lock_policy,
    SetSchedulerConfigurationV2Callback callback) {
  scheduler_configuration_name_ = config_name;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true, /*num_cores_disabled=*/0));
}

void FakeDebugDaemonClient::SetU2fFlags(const std::set<std::string>& flags,
                                        VoidDBusMethodCallback callback) {
  u2f_flags_ = flags;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::GetU2fFlags(
    DBusMethodCallback<std::set<std::string>> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::make_optional(u2f_flags_)));
}

}  // namespace chromeos
