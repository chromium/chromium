// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_FAKE_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_FAKE_DEBUG_DAEMON_CLIENT_H_

#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace ash {

// The DebugDaemonClient implementation used on Linux desktop,
// which does nothing.
class COMPONENT_EXPORT(DEBUG_DAEMON) FakeDebugDaemonClient
    : public DebugDaemonClient {
 public:
  FakeDebugDaemonClient();

  FakeDebugDaemonClient(const FakeDebugDaemonClient&) = delete;
  FakeDebugDaemonClient& operator=(const FakeDebugDaemonClient&) = delete;

  ~FakeDebugDaemonClient() override;

  void Init(dbus::Bus* bus) override;
  void DumpDebugLogs(bool is_compressed,
                     int file_descriptor,
                     chromeos::VoidDBusMethodCallback callback) override;
  void SetDebugMode(const std::string& subsystem,
                    chromeos::VoidDBusMethodCallback callback) override;
  std::string GetTracingAgentName() override;
  std::string GetTraceEventLabel() override;
  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         StartAgentTracingCallback callback) override;
  void StopAgentTracing(StopAgentTracingCallback callback) override;
  void SetStopAgentTracingTaskRunner(
      scoped_refptr<base::TaskRunner> task_runner) override;
  void GetRoutes(
      bool numeric,
      bool ipv6,
      bool all_tables,
      chromeos::DBusMethodCallback<std::vector<std::string>> callback) override;
  void SetKstaledRatio(uint8_t val, KstaledRatioCallback callback) override;
  void GetNetworkStatus(
      chromeos::DBusMethodCallback<std::string> callback) override;
  void GetNetworkInterfaces(
      chromeos::DBusMethodCallback<std::string> callback) override;
  void GetPerfOutput(const std::vector<std::string>& quipper_args,
                     bool disable_cpu_idle,
                     int file_descriptor,
                     chromeos::DBusMethodCallback<uint64_t> callback) override;
  void StopPerf(uint64_t session_id,
                chromeos::VoidDBusMethodCallback callback) override;
  void GetFeedbackLogs(
      const cryptohome::AccountIdentifier& id,
      const std::vector<debugd::FeedbackLogType>& requested_logs,
      GetLogsCallback callback) override;
  void GetFeedbackBinaryLogs(
      const cryptohome::AccountIdentifier& id,
      const std::map<debugd::FeedbackBinaryLogType, base::ScopedFD>&
          log_type_fds,
      chromeos::VoidDBusMethodCallback callback) override;
  void BackupArcBugReport(const cryptohome::AccountIdentifier& id,
                          chromeos::VoidDBusMethodCallback callback) override;
  void GetAllLogs(GetLogsCallback callback) override;
  void GetLog(const std::string& log_name,
              chromeos::DBusMethodCallback<std::string> callback) override;
  void TestICMP(const std::string& ip_address,
                TestICMPCallback callback) override;
  void TestICMPWithOptions(const std::string& ip_address,
                           const std::map<std::string, std::string>& options,
                           TestICMPCallback callback) override;
  void UploadCrashes(UploadCrashesCallback callback) override;
  void EnableDebuggingFeatures(const std::string& password,
                               EnableDebuggingCallback callback) override;
  void QueryDebuggingFeatures(QueryDevFeaturesCallback callback) override;
  void RemoveRootfsVerification(EnableDebuggingCallback callback) override;
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void SetOomScoreAdj(const std::map<pid_t, int32_t>& pid_to_oom_score_adj,
                      SetOomScoreAdjCallback callback) override;
  void CupsAddManuallyConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& language,
      const std::string& ppd_contents,
      CupsAddPrinterCallback callback) override;
  void CupsAddAutoConfiguredPrinter(const std::string& name,
                                    const std::string& uri,
                                    const std::string& language,
                                    CupsAddPrinterCallback callback) override;
  void CupsRemovePrinter(const std::string& name,
                         CupsRemovePrinterCallback callback,
                         base::OnceClosure error_callback) override;
  // Returns PPD set in CupsAddManuallyConfiguredPrinter or an empty string if
  // the printer was added with CupsAddAutoConfiguredPrinter. If the printer
  // does not exists then `error_callback` is called.
  void CupsRetrievePrinterPpd(const std::string& name,
                              CupsRetrievePrinterPpdCallback callback,
                              base::OnceClosure error_callback) override;
  void StartPluginVmDispatcher(const std::string& owner_id,
                               const std::string& lang,
                               PluginVmDispatcherCallback callback) override;
  void StopPluginVmDispatcher(PluginVmDispatcherCallback callback) override;
  void SetRlzPingSent(SetRlzPingSentCallback callback) override;
  void SetSchedulerConfigurationV2(
      const std::string& config_name,
      bool lock_policy,
      SetSchedulerConfigurationV2Callback callback) override;
  void SetU2fFlags(const std::set<std::string>& flags,
                   chromeos::VoidDBusMethodCallback callback) override;
  void GetU2fFlags(
      chromeos::DBusMethodCallback<std::set<std::string>> callback) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void PacketCaptureStartSignalReceived(dbus::Signal* signal) override;
  void PacketCaptureStopSignalReceived(dbus::Signal* signal) override;
  void StopPacketCapture(const std::string& handle) override;

  void BluetoothStartBtsnoop(BluetoothBtsnoopCallback callback) override;
  void BluetoothStopBtsnoop(int fd, BluetoothBtsnoopCallback callback) override;

  // Sets debugging features mask for testing.
  virtual void SetDebuggingFeaturesStatus(int features_mask);

  // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
  // pending callbacks if is_available is true.
  void SetServiceIsAvailable(bool is_available);

  // Sets routes that will be returned by GetRoutes() for testing.
  void SetRoutesForTesting(std::vector<std::string> routes);

  const std::string& scheduler_configuration_name() const {
    return scheduler_configuration_name_;
  }

  const std::set<std::string>& u2f_flags() const { return u2f_flags_; }

 private:
  int features_mask_;

  bool service_is_available_;
  std::vector<chromeos::WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;
  // Stores printer's name as a key and PPD content as a value.
  std::map<std::string, std::string> printers_;
  std::vector<std::string> routes_;
  std::string scheduler_configuration_name_;
  std::set<std::string> u2f_flags_;
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_FAKE_DEBUG_DAEMON_CLIENT_H_
