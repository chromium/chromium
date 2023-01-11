// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const char kCrOSTracingAgentName[] = "cros";
const char kCrOSTraceLabel[] = "systemTraceEvents";

}  // namespace

namespace ash {

FakeDebugDaemonClient::FakeDebugDaemonClient()
    : features_mask_(DebugDaemonClient::DEV_FEATURE_NONE),
      service_is_available_(true) {}

FakeDebugDaemonClient::~FakeDebugDaemonClient() = default;

void FakeDebugDaemonClient::Init(dbus::Bus* bus) {}

void FakeDebugDaemonClient::DumpDebugLogs(
    bool is_compressed,
    int file_descriptor,
    chromeos::VoidDBusMethodCallback callback) {
  std::move(callback).Run(true);
}

void FakeDebugDaemonClient::SetDebugMode(
    const std::string& subsystem,
    chromeos::VoidDBusMethodCallback callback) {
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
    chromeos::DBusMethodCallback<std::string> callback) {
  std::move(callback).Run(std::string());
}

void FakeDebugDaemonClient::SwapZramEnableWriteback(
    uint32_t size_mb,
    chromeos::DBusMethodCallback<std::string> callback) {
  std::move(callback).Run(std::string());
}

void FakeDebugDaemonClient::SwapZramSetWritebackLimit(
    uint32_t limit_pages,
    chromeos::DBusMethodCallback<std::string> callback) {
  std::move(callback).Run(std::string());
}

void FakeDebugDaemonClient::SwapZramMarkIdle(
    uint32_t age_seconds,
    chromeos::DBusMethodCallback<std::string> callback) {
  std::move(callback).Run(std::string());
}

void FakeDebugDaemonClient::InitiateSwapZramWriteback(
    debugd::ZramWritebackMode mode,
    chromeos::DBusMethodCallback<std::string> callback) {
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetTracingAgentName(),
                                true /* success */));
}

void FakeDebugDaemonClient::StopAgentTracing(
    StopAgentTracingCallback callback) {
  std::string trace_data = "# tracer: nop\n";
  std::move(callback).Run(
      GetTracingAgentName(), GetTraceEventLabel(),
      base::MakeRefCounted<base::RefCountedString>(std::move(trace_data)));
}

void FakeDebugDaemonClient::SetStopAgentTracingTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner) {}

void FakeDebugDaemonClient::SetRoutesForTesting(
    std::vector<std::string> routes) {
  routes_ = std::move(routes);
}

void FakeDebugDaemonClient::GetRoutes(
    bool numeric,
    bool ipv6,
    bool all_tables,
    chromeos::DBusMethodCallback<std::vector<std::string>> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), absl::make_optional(routes_)));
}

void FakeDebugDaemonClient::GetNetworkStatus(
    chromeos::DBusMethodCallback<std::string> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
}

void FakeDebugDaemonClient::GetNetworkInterfaces(
    chromeos::DBusMethodCallback<std::string> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
}

void FakeDebugDaemonClient::GetPerfOutput(
    const std::vector<std::string>& quipper_args,
    bool disable_cpu_idle,
    int file_descriptor,
    chromeos::DBusMethodCallback<uint64_t> error_callback) {}

void FakeDebugDaemonClient::StopPerf(
    uint64_t session_id,
    chromeos::VoidDBusMethodCallback callback) {}

void FakeDebugDaemonClient::GetFeedbackLogsV2(
    const cryptohome::AccountIdentifier& id,
    const std::vector<debugd::FeedbackLogType>& requested_logs,
    GetLogsCallback callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Log"] = "Your email address is abc@abc.com";
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), /*succeeded=*/true, sample));
}

void FakeDebugDaemonClient::BackupArcBugReport(
    const cryptohome::AccountIdentifier& id,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::GetAllLogs(GetLogsCallback callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Log"] = "Your email address is abc@abc.com";
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false, sample));
}

void FakeDebugDaemonClient::GetLog(
    const std::string& log_name,
    chromeos::DBusMethodCallback<std::string> callback) {
  std::string result = log_name + ": response from GetLog";
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void FakeDebugDaemonClient::TestICMP(const std::string& ip_address,
                                     TestICMPCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
}

void FakeDebugDaemonClient::TestICMPWithOptions(
    const std::string& ip_address,
    const std::map<std::string, std::string>& options,
    TestICMPCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), absl::nullopt));
}

void FakeDebugDaemonClient::UploadCrashes(UploadCrashesCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::EnableDebuggingFeatures(
    const std::string& password,
    EnableDebuggingCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::QueryDebuggingFeatures(
    QueryDevFeaturesCallback callback) {
  bool supported = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kSystemDevMode);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), true,
          static_cast<int>(
              supported ? features_mask_
                        : debugd::DevFeatureFlag::DEV_FEATURES_DISABLED)));
}

void FakeDebugDaemonClient::RemoveRootfsVerification(
    EnableDebuggingCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  if (service_is_available_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeDebugDaemonClient::SetOomScoreAdj(
    const std::map<pid_t, int32_t>& pid_to_oom_score_adj,
    SetOomScoreAdjCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true, ""));
}

void FakeDebugDaemonClient::SetDebuggingFeaturesStatus(int features_mask) {
  features_mask_ = features_mask;
}

void FakeDebugDaemonClient::SetServiceIsAvailable(bool is_available) {
  service_is_available_ = is_available;
  if (!is_available)
    return;

  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0));
}

void FakeDebugDaemonClient::CupsAddAutoConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    CupsAddPrinterCallback callback) {
  printers_.insert(name);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0));
}

void FakeDebugDaemonClient::CupsRemovePrinter(
    const std::string& name,
    CupsRemovePrinterCallback callback,
    base::OnceClosure error_callback) {
  const bool has_printer = base::Contains(printers_, name);
  if (has_printer)
    printers_.erase(name);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), has_printer));
}

void FakeDebugDaemonClient::CupsRetrievePrinterPpd(
    const std::string& name,
    CupsRetrievePrinterPpdCallback callback,
    base::OnceClosure error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), ppd_data_));
}

void FakeDebugDaemonClient::SetPpdDataForTesting(
    const std::vector<uint8_t>& data) {
  ppd_data_ = data;
}

void FakeDebugDaemonClient::StartPluginVmDispatcher(
    const std::string& /* owner_id */,
    const std::string& /* lang */,
    PluginVmDispatcherCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::StopPluginVmDispatcher(
    PluginVmDispatcherCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::SetRlzPingSent(SetRlzPingSentCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::SetSchedulerConfigurationV2(
    const std::string& config_name,
    bool lock_policy,
    SetSchedulerConfigurationV2Callback callback) {
  scheduler_configuration_name_ = config_name;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true, /*num_cores_disabled=*/0));
}

void FakeDebugDaemonClient::SetU2fFlags(
    const std::set<std::string>& flags,
    chromeos::VoidDBusMethodCallback callback) {
  u2f_flags_ = flags;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::GetU2fFlags(
    chromeos::DBusMethodCallback<std::set<std::string>> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), absl::make_optional(u2f_flags_)));
}

void FakeDebugDaemonClient::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void FakeDebugDaemonClient::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void FakeDebugDaemonClient::PacketCaptureStartSignalReceived(
    dbus::Signal* signal) {
  for (auto& observer : observers_)
    observer.OnPacketCaptureStarted();
}

void FakeDebugDaemonClient::PacketCaptureStopSignalReceived(
    dbus::Signal* signal) {
  for (auto& observer : observers_)
    observer.OnPacketCaptureStopped();
}

void FakeDebugDaemonClient::StopPacketCapture(const std::string& handle) {
  // Act like PacketCaptureStop signal is received.
  PacketCaptureStopSignalReceived(nullptr);
}

}  // namespace ash
