// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_H_

#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task_runner.h"
#include "base/trace_event/tracing_agent.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {
class AccountIdentifier;
}
namespace chromeos {

// A DbusLibraryError represents an error response received from D-Bus.
enum DbusLibraryError {
  kGenericError = -1,  // Catch-all generic error
  kNoReply = -2,       // debugd did not respond before timeout
  kTimeout = -3        // Unspecified D-Bus timeout (e.g. socket error)
};

// DebugDaemonClient is used to communicate with the debug daemon.
class COMPONENT_EXPORT(DEBUG_DAEMON) DebugDaemonClient
    : public DBusClient,
      public base::trace_event::TracingAgent {
 public:
  ~DebugDaemonClient() override;

  // Requests to store debug logs into |file_descriptor| and calls |callback|
  // when completed. Debug logs will be stored in the .tgz if
  // |is_compressed| is true, otherwise in logs will be stored in .tar format.
  // This method duplicates |file_descriptor| so it's OK to close the FD without
  // waiting for the result.
  virtual void DumpDebugLogs(bool is_compressed,
                             int file_descriptor,
                             VoidDBusMethodCallback callback) = 0;

  // Requests to change debug mode to given |subsystem| and calls
  // |callback| when completed. |subsystem| should be one of the
  // following: "wifi", "ethernet", "cellular" or "none".
  virtual void SetDebugMode(const std::string& subsystem,
                            VoidDBusMethodCallback callback) = 0;

  // Gets information about routes.
  virtual void GetRoutes(
      bool numeric,
      bool ipv6,
      DBusMethodCallback<std::vector<std::string> /* routes */> callback) = 0;

  // Gets information about network status as json.
  virtual void GetNetworkStatus(DBusMethodCallback<std::string> callback) = 0;

  // Gets information about network interfaces as json.
  // For details, please refer to
  // http://gerrit.chromium.org/gerrit/#/c/28045/5/src/helpers/netif.cc
  virtual void GetNetworkInterfaces(
      DBusMethodCallback<std::string> callback) = 0;

  // Runs perf (via quipper) with arguments for |duration| (converted to
  // seconds) and returns data collected over the passed |file_descriptor|.
  // |callback| is called on the completion of the D-Bus call.
  // Note that quipper failures may occur after successfully running the D-Bus
  // method. Such errors can be detected by |file_descriptor| and all its
  // duplicates being closed with no data written.
  // This method duplicates |file_descriptor| so it's OK to close the FD without
  // waiting for the result.
  virtual void GetPerfOutput(base::TimeDelta duration,
                             const std::vector<std::string>& perf_args,
                             int file_descriptor,
                             DBusMethodCallback<uint64_t> callback) = 0;

  // Stops the perf session identified with |session_id| that was started by a
  // prior call to GetPerfOutput(), and let the caller of GetPerfOutput() gather
  // profiling data right away. If the profiler session as identified by
  // |session_id| has ended, this method will silently succeed.
  virtual void StopPerf(uint64_t session_id,
                        VoidDBusMethodCallback callback) = 0;

  // Callback type for GetAllLogs()
  using GetLogsCallback =
      base::OnceCallback<void(bool succeeded,
                              const std::map<std::string, std::string>& logs)>;

  // Gets the scrubbed logs from debugd that are very large and cannot be
  // returned directly from D-Bus. These logs will include ARC and cheets
  // system information.
  // |id|: Cryptohome Account identifier for the user to get
  // logs for.
  virtual void GetScrubbedBigLogs(const cryptohome::AccountIdentifier& id,
                                  GetLogsCallback callback) = 0;

  // Retrieves the ARC bug report for user identified by |userhash|
  // and saves it in debugd daemon store.
  // If a backup already exists, it is overwritten.
  // If backup operation fails, an error is logged.
  // |id|: Cryptohome Account identifier for the user to get
  // logs for.
  virtual void BackupArcBugReport(const cryptohome::AccountIdentifier& id,
                                  VoidDBusMethodCallback callback) = 0;

  // Gets all logs collected by debugd.
  virtual void GetAllLogs(GetLogsCallback callback) = 0;

  // Gets an individual log source provided by debugd.
  virtual void GetLog(const std::string& log_name,
                      DBusMethodCallback<std::string> callback) = 0;

  virtual void SetStopAgentTracingTaskRunner(
      scoped_refptr<base::TaskRunner> task_runner) = 0;

  using KstaledRatioCallback = base::OnceCallback<void(bool)>;

  // Sets the kstaled ratio to the provided value, for more information
  // see chromeos/memory/README.md.
  virtual void SetKstaledRatio(uint8_t val, KstaledRatioCallback) = 0;

  // Called once TestICMP() is complete. Takes an optional string.
  // - The optional string has value if information was obtained successfully.
  // - The string value contains information about ICMP connectivity to a
  //   specified host as json.
  //   For details please refer to
  //   https://gerrit.chromium.org/gerrit/#/c/30310/2/src/helpers/icmp.cc
  using TestICMPCallback = DBusMethodCallback<std::string>;

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
      WaitForServiceToBeAvailableCallback callback) = 0;

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
  // name. |uri| is the device.  |ppd_contents| is the contents of the
  // PPD file used to drive the device.  |callback| is called with
  // true if adding the printer to CUPS was successful and false if
  // there was an error.  |error_callback| will be called if there was
  // an error in communicating with debugd.
  virtual void CupsAddManuallyConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& ppd_contents,
      CupsAddPrinterCallback callback) = 0;

  // Calls CupsAddAutoConfiguredPrinter.  |name| is the printer
  // name. |uri| is the device.  |callback| is called with true if
  // adding the printer to CUPS was successful and false if there was
  // an error.  |error_callback| will be called if there was an error
  // in communicating with debugd.
  virtual void CupsAddAutoConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
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
                           VoidDBusMethodCallback callback) = 0;
  // Get U2F flags.
  virtual void GetU2fFlags(
      DBusMethodCallback<std::set<std::string>> callback) = 0;

  // Set Swap Parameter
  virtual void SetSwapParameter(const std::string& parameter,
                                int32_t value,
                                DBusMethodCallback<std::string> callback) = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static std::unique_ptr<DebugDaemonClient> Create();

 protected:
  // For calling Init() in initiating a DebugDaemonClient instance for private
  // connections.
  friend class DebugDaemonClientProvider;

  // Create() should be used instead.
  DebugDaemonClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(DebugDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DEBUG_DAEMON_DEBUG_DAEMON_CLIENT_H_
