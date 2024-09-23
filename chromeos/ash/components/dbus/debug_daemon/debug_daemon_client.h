// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_H_

#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/observer_list_types.h"
#include "base/task/task_runner.h"
#include "base/trace_event/tracing_agent.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {
class AccountIdentifier;
}

namespace ash {

// DebugDaemonClient is used to communicate with the debug daemon.
class COMPONENT_EXPORT(DEBUG_DAEMON) DebugDaemonClient
    : public chromeos::DBusClient,
      public base::trace_event::TracingAgent {
 public:
  // Returns the global instance if initialized. May return null.
  static DebugDaemonClient* Get();

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Sets a temporary instance for testing. Overrides the existing
  // global instance, if any.
  static void SetInstanceForTest(DebugDaemonClient* client);

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  DebugDaemonClient(const DebugDaemonClient&) = delete;
  DebugDaemonClient& operator=(const DebugDaemonClient&) = delete;

  ~DebugDaemonClient() override;

  // Observes the signals that are received from D-Bus.
  class Observer : public base::CheckedObserver {
   public:
    // Called when a PacketCaptureStart signal is received through D-Bus.
    virtual void OnPacketCaptureStarted() {}

    // Called when a PacketCaptureStop signal is received through D-Bus.
    virtual void OnPacketCaptureStopped() {}
  };

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Requests to store debug logs into |file_descriptor| and calls |callback|
  // when completed. Debug logs will be stored in the .tgz if
  // |is_compressed| is true, otherwise in logs will be stored in .tar format.
  // This method duplicates |file_descriptor| so it's OK to close the FD without
  // waiting for the result.
  virtual void DumpDebugLogs(bool is_compressed,
                             int file_descriptor,
                             chromeos::VoidDBusMethodCallback callback) = 0;

  // Requests to change debug mode to given |subsystem| and calls
  // |callback| when completed. |subsystem| should be one of the
  // following: "wifi", "ethernet", "cellular" or "none".
  virtual void SetDebugMode(const std::string& subsystem,
                            chromeos::VoidDBusMethodCallback callback) = 0;

  // Gets information about routes.
  virtual void GetRoutes(
      bool numeric,
      bool ipv6,
      bool all_tables,
      chromeos::DBusMethodCallback<std::vector<std::string> /* routes */>
          callback) = 0;

  // Gets information about network status as json.
  virtual void GetNetworkStatus(
      chromeos::DBusMethodCallback<std::string> callback) = 0;

  // Gets information about network interfaces as json.
  // For details, please refer to
  // http://gerrit.chromium.org/gerrit/#/c/28045/5/src/helpers/netif.cc
  virtual void GetNetworkInterfaces(
      chromeos::DBusMethodCallback<std::string> callback) = 0;

  // Runs perf (via quipper) with |quipper_args| and returns data collected
  // over the passed |file_descriptor|.
  // |callback| is called on the completion of the D-Bus call.
  // Note that quipper failures may occur after successfully running the D-Bus
  // method. Such errors can be detected by |file_descriptor| and all its
  // duplicates being closed with no data written.
  // This method duplicates |file_descriptor| so it's OK to close the FD without
  // waiting for the result.
  virtual void GetPerfOutput(
      const std::vector<std::string>& quipper_args,
      bool disable_cpu_idle,
      int file_descriptor,
      chromeos::DBusMethodCallback<uint64_t> callback) = 0;

  // Stops the perf session identified with |session_id| that was started by a
  // prior call to GetPerfOutput(), and let the caller of GetPerfOutput() gather
  // profiling data right away. If the profiler session as identified by
  // |session_id| has ended, this method will silently succeed.
  virtual void StopPerf(uint64_t session_id,
                        chromeos::VoidDBusMethodCallback callback) = 0;

  // Callback type for GetAllLogs()
  using GetLogsCallback =
      base::OnceCallback<void(bool succeeded,
                              const std::map<std::string, std::string>& logs)>;

  // Gets feedback logs from debugd that are very large and cannot be
  // returned directly from D-Bus. These logs will include ARC and cheets
  // system information.
  // |id|: Cryptohome Account identifier for the user to get
  // logs for.
  // |requested_logs|: The list of requested logs. All available logs will be
  // requested if left empty.
  virtual void GetFeedbackLogs(
      const cryptohome::AccountIdentifier& id,
      const std::vector<debugd::FeedbackLogType>& requested_logs,
      GetLogsCallback callback) = 0;

  // Gets feedback binary logs from debugd.
  // |id|: Cryptohome Account identifier for the user to get logs for.
  // |log_type_fds|: The map of FeedbackBinaryLogType and its FD pair.
  // |callback|: The callback to be invoked once the debugd method is completed.
  virtual void GetFeedbackBinaryLogs(
      const cryptohome::AccountIdentifier& id,
      const std::map<debugd::FeedbackBinaryLogType, base::ScopedFD>&
          log_type_fds,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Retrieves the ARC bug report for user identified by |userhash|
  // and saves it in debugd daemon store.
  // If a backup already exists, it is overwritten.
  // If backup operation fails, an error is logged.
  // |id|: Cryptohome Account identifier for the user to get
  // logs for.
  virtual void BackupArcBugReport(
      const cryptohome::AccountIdentifier& id,
      chromeos::VoidDBusMethodCallback callback) = 0;

  // Gets all logs collected by debugd.
  virtual void GetAllLogs(GetLogsCallback callback) = 0;

  // Gets an individual log source provided by debugd.
  virtual void GetLog(const std::string& log_name,
                      chromeos::DBusMethodCallback<std::string> callback) = 0;

  virtual void SetStopAgentTracingTaskRunner(
      scoped_refptr<base::TaskRunner> task_runner) = 0;

  using KstaledRatioCallback = base::OnceCallback<void(bool)>;

  // Sets the kstaled ratio to the provided value, for more information
  // see chromeos/ash/components/memory/README.md.
  virtual void SetKstaledRatio(uint8_t val, KstaledRatioCallback) = 0;

  // Called once TestICMP() is complete. Takes an optional string.
  // - The optional string has value if information was obtained successfully.
  // - The string value contains information about ICMP connectivity to a
  //   specified host as json.
  //   For details please refer to
  //   https://gerrit.chromium.org/gerrit/#/c/30310/2/src/helpers/icmp.cc
  using TestICMPCallback = chromeos::DBusMethodCallback<std::string>;

  // Tests ICMP connectivity to a specified host. The |ip_address| contains the
  // IPv4 or IPv6 address of the host, for example "8.8.8.8".
  virtual void TestICMP(const std::string& ip_address,
                        TestICMPCallback callback) = 0;

  // Tests ICMP connectivity to a specified host. The |ip_address| contains the
  // IPv4 or IPv6 address of the host, for example "8.8.8.8".
  virtual void TestICMPWithOptions(
      const std::string& ip_address,
      const std::map<std::string, std::string>& options,
      TestICMPCallback callback) = 0;

  // Called once EnableDebuggingFeatures() is complete. |succeeded| will be true
  // if debugging features have been successfully enabled.
  using EnableDebuggingCallback = base::OnceCallback<void(bool succeeded)>;

  // Enables debugging features (sshd, boot from USB). |password| is a new
  // password for root user. Can be only called in dev mode.
  virtual void EnableDebuggingFeatures(const std::string& password,
                                       EnableDebuggingCallback callback) = 0;

  static const int DEV_FEATURE_NONE = 0;
  static const int DEV_FEATURE_ALL_ENABLED =
      debugd::DevFeatureFlag::DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED |
      debugd::DevFeatureFlag::DEV_FEATURE_BOOT_FROM_USB_ENABLED |
      debugd::DevFeatureFlag::DEV_FEATURE_SSH_SERVER_CONFIGURED |
      debugd::DevFeatureFlag::DEV_FEATURE_DEV_MODE_ROOT_PASSWORD_SET;

  // Called once QueryDebuggingFeatures() is complete. |succeeded| will be true
  // if debugging features have been successfully enabled. |feature_mask| is a
  // bitmask made out of DebuggingFeature enum values.
  using QueryDevFeaturesCallback =
      base::OnceCallback<void(bool succeeded, int feature_mask)>;
  // Checks which debugging features have been already enabled.
  virtual void QueryDebuggingFeatures(QueryDevFeaturesCallback callback) = 0;

  // Removes rootfs verification from the file system. Can be only called in
  // dev mode.
  virtual void RemoveRootfsVerification(EnableDebuggingCallback callback) = 0;

  using UploadCrashesCallback = base::OnceCallback<void(bool succeeded)>;
  // Trigger uploading of crashes.
  virtual void UploadCrashes(UploadCrashesCallback callback) = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) = 0;

  // A callback for SetOomScoreAdj().
  using SetOomScoreAdjCallback =
      base::OnceCallback<void(bool success, const std::string& output)>;

  // Set OOM score oom_score_adj for some process.
  // Note that the corresponding DBus configuration of the debugd method
  // "SetOomScoreAdj" only permits setting OOM score for processes running by
  // user chronos or Android apps.
  virtual void SetOomScoreAdj(
      const std::map<pid_t, int32_t>& pid_to_oom_score_adj,
      SetOomScoreAdjCallback callback) = 0;

  // A callback to handle the result of CupsAdd[Auto|Manually]ConfiguredPrinter.
  // A negative value denotes a D-Bus library error while non-negative values
  // denote a response from debugd.
  using CupsAddPrinterCallback = base::OnceCallback<void(int32_t)>;

  // Calls CupsAddManuallyConfiguredPrinter.  |name| is the printer
  // name. |uri| is the device.  |language| is the locale code for the
  // user's language, e.g., "en-us" or "jp".  |ppd_contents| is the
  // contents of the PPD file used to drive the device.  |callback| is
  // called with true if adding the printer to CUPS was successful and
  // false if there was an error.  |error_callback| will be called if
  // there was an error in communicating with debugd.
  virtual void CupsAddManuallyConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& language,
      const std::string& ppd_contents,
      CupsAddPrinterCallback callback) = 0;

  // Calls CupsAddAutoConfiguredPrinter.  |name| is the printer
  // name. |uri| is the device.  |language| is the locale code for the
  // user's language, e.g., "en-us" or "jp".  |callback| is called with
  // true if adding the printer to CUPS was successful and false if there
  // was an error.  |error_callback| will be called if there was an error
  // in communicating with debugd.
  virtual void CupsAddAutoConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& language,
      CupsAddPrinterCallback callback) = 0;

  // A callback to handle the result of CupsRemovePrinter.
  using CupsRemovePrinterCallback = base::OnceCallback<void(bool success)>;

  // Calls CupsRemovePrinter.  |name| is the printer name as registered in
  // CUPS.  |callback| is called with true if removing the printer from CUPS was
  // successful and false if there was an error.  |error_callback| will be
  // called if there was an error in communicating with debugd.
  virtual void CupsRemovePrinter(const std::string& name,
                                 CupsRemovePrinterCallback callback,
                                 base::OnceClosure error_callback) = 0;

  // A callback to handle the result of CupsRetrievePrinterPpd.
  using CupsRetrievePrinterPpdCallback =
      base::OnceCallback<void(const std::vector<uint8_t>& ppd)>;

  // Calls the debugd method to retrieve a PPD.  |name| is the printer name as
  // registered in CUPS. |callback| is called with a string containing the PPD
  // data. |error_callback| will be called if there was an error retrieving the
  // PPD.
  virtual void CupsRetrievePrinterPpd(const std::string& name,
                                      CupsRetrievePrinterPpdCallback callback,
                                      base::OnceClosure error_callback) = 0;

  // A callback to handle the result of
  // StartPluginVmDispatcher/StopPluginVmDispatcher.
  using PluginVmDispatcherCallback = base::OnceCallback<void(bool success)>;
  // Calls debugd::kStartVmPluginDispatcher, which starts the PluginVm
  // dispatcher service on behalf of |owner_id|. |lang| indicates
  // currently selected system language. |callback| is called
  // when the method finishes.
  virtual void StartPluginVmDispatcher(const std::string& owner_id,
                                       const std::string& lang,
                                       PluginVmDispatcherCallback callback) = 0;
  // Calls debug::kStopVmPluginDispatcher, which stops the PluginVm dispatcher
  // service. |callback| is called when the method finishes.
  virtual void StopPluginVmDispatcher(PluginVmDispatcherCallback callback) = 0;

  // A callback to handle the result of SetRlzPingSent.
  using SetRlzPingSentCallback = base::OnceCallback<void(bool success)>;
  // Calls debugd::kSetRlzPingSent, which sets |should_send_rlz_ping| in RW_VPD
  // to 0.
  virtual void SetRlzPingSent(SetRlzPingSentCallback callback) = 0;

  // A callback to handle the result of SetSchedulerConfigurationV2.
  using SetSchedulerConfigurationV2Callback =
      base::OnceCallback<void(bool success, size_t num_cores_disabled)>;
  // Request switching to the scheduler configuration profile indicated. The
  // profile names are defined by debugd, which adjusts various knobs affecting
  // kernel level task scheduling (see debugd source code for details). When
  // |lock_policy| is true, the policy is locked until the device is rebooted.
  virtual void SetSchedulerConfigurationV2(
      const std::string& config_name,
      bool lock_policy,
      SetSchedulerConfigurationV2Callback callback) = 0;

  // Set U2F flags.
  virtual void SetU2fFlags(const std::set<std::string>& flags,
                           chromeos::VoidDBusMethodCallback callback) = 0;
  // Get U2F flags.
  virtual void GetU2fFlags(
      chromeos::DBusMethodCallback<std::set<std::string>> callback) = 0;

  // Stops the packet capture process identified with |handle|. |handle| is a
  // unique process identifier that is returned from debugd's PacketCaptureStart
  // D-Bus method when the packet capture process is started. Stops all on-going
  // packet capture operations if the |handle| is empty.
  virtual void StopPacketCapture(const std::string& handle) = 0;

  virtual void PacketCaptureStartSignalReceived(dbus::Signal* signal) = 0;
  virtual void PacketCaptureStopSignalReceived(dbus::Signal* signal) = 0;

  // A callback to handle the result of
  // BluetoothStartBtsnoop/BluetoothStopBtsnoop.
  using BluetoothBtsnoopCallback = base::OnceCallback<void(bool success)>;
  // Starts capturing btsnoop logs, which is kept inside daemon-store
  virtual void BluetoothStartBtsnoop(BluetoothBtsnoopCallback callback) = 0;
  // Stops capturing btsnoop logs and copy it to the Downloads directory.
  virtual void BluetoothStopBtsnoop(int fd,
                                    BluetoothBtsnoopCallback callback) = 0;

 protected:
  // For creating a second instance of DebugDaemonClient on another thread for
  // private connections.
  friend class DebugDaemonClientProvider;

  // Initialize() should be used instead.
  DebugDaemonClient();

  // See DebugDaemonClientProvider for details.
  static std::unique_ptr<DebugDaemonClient> CreateInstance();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_H_
