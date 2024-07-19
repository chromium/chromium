// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chromeos/dbus/constants/dbus_switches.h"

namespace {

const char kCrOSTracingAgentName[] = "cros";
const char kCrOSTraceLabel[] = "systemTraceEvents";

// Writes the |data| to |fd|, then close |fd|.
void WriteData(base::ScopedFD fd, const std::string& data) {
  base::WriteFileDescriptor(fd.get(), data);
}

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
      base::BindOnce(std::move(callback), std::make_optional(routes_)));
}

void FakeDebugDaemonClient::GetNetworkStatus(
    chromeos::DBusMethodCallback<std::string> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

void FakeDebugDaemonClient::GetNetworkInterfaces(
    chromeos::DBusMethodCallback<std::string> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

void FakeDebugDaemonClient::GetPerfOutput(
    const std::vector<std::string>& quipper_args,
    bool disable_cpu_idle,
    int file_descriptor,
    chromeos::DBusMethodCallback<uint64_t> error_callback) {}

void FakeDebugDaemonClient::StopPerf(
    uint64_t session_id,
    chromeos::VoidDBusMethodCallback callback) {}

void FakeDebugDaemonClient::GetFeedbackLogs(
    const cryptohome::AccountIdentifier& id,
    const std::vector<debugd::FeedbackLogType>& requested_logs,
    GetLogsCallback callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Log"] = "Your email address is abc@abc.com";
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), /*succeeded=*/true, sample));
}

void FakeDebugDaemonClient::GetFeedbackBinaryLogs(
    const cryptohome::AccountIdentifier& id,
    const std::map<debugd::FeedbackBinaryLogType, base::ScopedFD>& log_type_fds,
    chromeos::VoidDBusMethodCallback callback) {
  constexpr char kTestData[] = "TestData";
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*succeeded=*/true));

  // Write dummy data to the pipes after callback is invoked to simulate
  // potential delay writing bug chunk of data.
  for (const auto& item : log_type_fds) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::BindOnce(&WriteData, base::ScopedFD(dup(item.second.get())),
                       kTestData));
  }
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
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

void FakeDebugDaemonClient::TestICMPWithOptions(
    const std::string& ip_address,
    const std::map<std::string, std::string>& options,
    TestICMPCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
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
    const std::string& language,
    const std::string& ppd_contents,
    CupsAddPrinterCallback callback) {
  printers_.insert_or_assign(name, ppd_contents);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), 0));
}

void FakeDebugDaemonClient::CupsAddAutoConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    const std::string& language,
    CupsAddPrinterCallback callback) {
  printers_.insert_or_assign(name, "");
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
  auto it = printers_.find(name);
  if (it == printers_.end()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(error_callback));
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::vector<uint8_t>(it->second.begin(),
                                                     it->second.end())));
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
      base::BindOnce(std::move(callback), std::make_optional(u2f_flags_)));
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

void FakeDebugDaemonClient::BluetoothStartBtsnoop(
    BluetoothBtsnoopCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeDebugDaemonClient::BluetoothStopBtsnoop(
    int fd,
    BluetoothBtsnoopCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace ash
