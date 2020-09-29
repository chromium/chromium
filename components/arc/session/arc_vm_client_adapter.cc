// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#include <deque>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_session.h"
#include "components/arc/session/file_system_status.h"
#include "components/version_info/version_info.h"

namespace arc {
namespace {

// The "_2d" in job names below corresponds to "-". Upstart escapes characters
// that aren't valid in D-Bus object paths with underscore followed by its
// ascii code in hex. So "arc_2dcreate_2ddata" becomes "arc-create-data".
constexpr const char kArcCreateDataJobName[] = "arc_2dcreate_2ddata";
constexpr const char kArcHostClockServiceJobName[] =
    "arc_2dhost_2dclock_2dservice";
constexpr const char kArcKeymasterJobName[] = "arc_2dkeymasterd";
constexpr const char kArcSensorServiceJobName[] = "arc_2dsensor_2dservice";
constexpr const char kArcVmMountMyFilesJobName[] = "arcvm_2dmount_2dmyfiles";
constexpr const char kArcVmMountRemovableMediaJobName[] =
    "arcvm_2dmount_2dremovable_2dmedia";
constexpr const char kArcVmServerProxyJobName[] = "arcvm_2dserver_2dproxy";
constexpr const char kArcVmAdbdJobName[] = "arcvm_2dadbd";
constexpr const char kArcVmPerBoardFeaturesJobName[] =
    "arcvm_2dper_2dboard_2dfeatures";
constexpr const char kArcVmBootNotificationServerJobName[] =
    "arcvm_2dboot_2dnotification_2dserver";

constexpr const char kCrosSystemPath[] = "/usr/bin/crossystem";
constexpr const char kArcVmBootNotificationServerSocketPath[] =
    "/run/arcvm_boot_notification_server/host.socket";

constexpr base::TimeDelta kArcBugReportBackupTimeMetricMinTime =
    base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kArcBugReportBackupTimeMetricMaxTime =
    base::TimeDelta::FromSeconds(60);
constexpr int kArcBugReportBackupTimeMetricBuckets = 50;
constexpr const char kArcBugReportBackupTimeMetric[] =
    "Login.ArcBugReportBackupTime";

constexpr int64_t kInvalidCid = -1;

constexpr base::TimeDelta kConnectTimeoutLimit =
    base::TimeDelta::FromSeconds(20);
constexpr base::TimeDelta kConnectSleepDurationInitial =
    base::TimeDelta::FromMilliseconds(100);

base::Optional<base::TimeDelta> g_connect_timeout_limit_for_testing;
base::Optional<base::TimeDelta> g_connect_sleep_duration_initial_for_testing;
bool g_enable_adb_over_usb_for_testing = false;

chromeos::ConciergeClient* GetConciergeClient() {
  return chromeos::DBusThreadManager::Get()->GetConciergeClient();
}

chromeos::DebugDaemonClient* GetDebugDaemonClient() {
  return chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
}

std::string GetChromeOsChannelFromLsbRelease() {
  constexpr const char kChromeOsReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
  constexpr const char kUnknown[] = "unknown";
  const std::string kChannelSuffix = "-channel";

  std::string value;
  base::SysInfo::GetLsbReleaseValue(kChromeOsReleaseTrack, &value);

  if (!base::EndsWith(value, kChannelSuffix, base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Unknown ChromeOS channel: \"" << value << "\"";
    return kUnknown;
  }
  return value.erase(value.find(kChannelSuffix), kChannelSuffix.size());
}

std::string MonotonicTimestamp() {
  struct timespec ts;
  const int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DPCHECK(ret == 0);
  const int64_t time =
      ts.tv_sec * base::Time::kNanosecondsPerSecond + ts.tv_nsec;
  return base::NumberToString(time);
}

ArcBinaryTranslationType IdentifyBinaryTranslationType(
    const StartParams& start_params) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  const bool is_houdini_available =
      command_line->HasSwitch(chromeos::switches::kEnableHoudini) ||
      command_line->HasSwitch(chromeos::switches::kEnableHoudini64);
  const bool is_ndk_translation_available =
      command_line->HasSwitch(chromeos::switches::kEnableNdkTranslation) ||
      command_line->HasSwitch(chromeos::switches::kEnableNdkTranslation64);

  if (!is_houdini_available && !is_ndk_translation_available)
    return ArcBinaryTranslationType::NONE;

  const bool prefer_ndk_translation =
      !is_houdini_available || start_params.native_bridge_experiment;

  if (is_ndk_translation_available && prefer_ndk_translation)
    return ArcBinaryTranslationType::NDK_TRANSLATION;

  return ArcBinaryTranslationType::HOUDINI;
}

std::vector<std::string> GenerateKernelCmdline(
    const StartParams& start_params,
    const UpgradeParams& upgrade_params,
    const FileSystemStatus& file_system_status,
    bool is_dev_mode,
    bool is_host_on_vm,
    const std::string& channel,
    const std::string& serial_number) {
  DCHECK(!serial_number.empty());

  std::string native_bridge;
  switch (IdentifyBinaryTranslationType(start_params)) {
    case ArcBinaryTranslationType::NONE:
      native_bridge = "0";
      break;
    case ArcBinaryTranslationType::HOUDINI:
      native_bridge = "libhoudini.so";
      break;
    case ArcBinaryTranslationType::NDK_TRANSLATION:
      native_bridge = "libndk_translation.so";
      break;
  }

  std::vector<std::string> result = {
      "androidboot.hardware=bertha",
      "androidboot.container=1",
      base::StringPrintf("androidboot.native_bridge=%s", native_bridge.c_str()),
      base::StringPrintf("androidboot.dev_mode=%d", is_dev_mode),
      base::StringPrintf("androidboot.disable_runas=%d", !is_dev_mode),
      base::StringPrintf("androidboot.host_is_in_vm=%d", is_host_on_vm),
      base::StringPrintf("androidboot.debuggable=%d",
                         file_system_status.is_android_debuggable()),
      base::StringPrintf("androidboot.lcd_density=%d",
                         start_params.lcd_density),
      base::StringPrintf("androidboot.arc_file_picker=%d",
                         start_params.arc_file_picker_experiment),
      base::StringPrintf("androidboot.arc_custom_tabs=%d",
                         start_params.arc_custom_tabs_experiment),
      base::StringPrintf("androidboot.disable_system_default_app=%d",
                         start_params.arc_disable_system_default_app),
      "androidboot.chromeos_channel=" + channel,
      "androidboot.boottime_offset=" + MonotonicTimestamp(),
  };
  // Since we don't do mini VM yet, set not only |start_params| but also
  // |upgrade_params| here for now.
  const std::vector<std::string> upgrade_props =
      GenerateUpgradeProps(upgrade_params, serial_number, "androidboot");
  result.insert(result.end(), upgrade_props.begin(), upgrade_props.end());

  // TODO(niwa): Check if we need to set ro.boot.enable_adb_sideloading for
  // ARCVM.

  // Conditionally sets some properties based on |start_params|.
  switch (start_params.play_store_auto_update) {
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT:
      break;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON:
      result.push_back("androidboot.play_store_auto_update=1");
      break;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF:
      result.push_back("androidboot.play_store_auto_update=0");
      break;
  }

  return result;
}

vm_tools::concierge::StartArcVmRequest CreateStartArcVmRequest(
    const std::string& user_id_hash,
    uint32_t cpus,
    const base::FilePath& demo_session_apps_path,
    const FileSystemStatus& file_system_status,
    std::vector<std::string> kernel_cmdline) {
  vm_tools::concierge::StartArcVmRequest request;

  request.set_name(kArcVmName);
  request.set_owner_id(user_id_hash);

  request.add_params("root=/dev/vda");
  if (file_system_status.is_host_rootfs_writable() &&
      file_system_status.is_system_image_ext_format()) {
    request.add_params("rw");
  }
  request.add_params("init=/init");

  for (auto& entry : kernel_cmdline)
    request.add_params(std::move(entry));

  vm_tools::concierge::VirtualMachineSpec* vm = request.mutable_vm();

  vm->set_kernel(file_system_status.guest_kernel_path().value());

  // Add rootfs as /dev/vda.
  vm->set_rootfs(file_system_status.system_image_path().value());
  request.set_rootfs_writable(file_system_status.is_host_rootfs_writable() &&
                              file_system_status.is_system_image_ext_format());

  // Add /vendor as /dev/block/vdb. The device name has to be consistent with
  // the one in GenerateFirstStageFstab() in ../arc_util.cc.
  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(file_system_status.vendor_image_path().value());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);

  // Add /run/imageloader/.../android_demo_apps.squash as /dev/block/vdc if
  // needed.
  // TODO(b/144542975): Do this on upgrade instead.
  if (!demo_session_apps_path.empty()) {
    disk_image = request.add_disks();
    disk_image->set_path(demo_session_apps_path.value());
    disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
    disk_image->set_writable(false);
    disk_image->set_do_mount(true);
  }

  // Add Android fstab.
  request.set_fstab(file_system_status.fstab_path().value());

  // Add cpus.
  request.set_cpus(cpus);

  // Add ignore_dev_conf setting for dev mode.
  request.set_ignore_dev_conf(IsArcVmDevConfIgnored());

  return request;
}

// Gets a system property managed by crossystem. This function can be called
// only with base::MayBlock().
int GetSystemPropertyInt(const std::string& property) {
  std::string output;
  if (!base::GetAppOutput({kCrosSystemPath, property}, &output))
    return -1;
  int output_int;
  return base::StringToInt(output, &output_int) ? output_int : -1;
}

const sockaddr_un* GetArcVmBootNotificationServerAddress() {
  static struct sockaddr_un address {
    .sun_family = AF_UNIX,
    .sun_path = "/run/arcvm_boot_notification_server/host.socket"
  };
  return &address;
}

// Connects to UDS socket at |kArcVmBootNotificationServerSocketPath|.
// Returns the connected socket fd if successful, or else an invalid fd. This
// function can only be called with base::MayBlock().
base::ScopedFD ConnectToArcVmBootNotificationServer() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  base::ScopedFD fd(socket(AF_UNIX, SOCK_STREAM, 0));
  DCHECK(fd.is_valid());

  if (HANDLE_EINTR(connect(fd.get(),
                           reinterpret_cast<const sockaddr*>(
                               GetArcVmBootNotificationServerAddress()),
                           sizeof(sockaddr_un)))) {
    PLOG(ERROR) << "Unable to connect to "
                << kArcVmBootNotificationServerSocketPath;
    return {};
  }

  return fd;
}

// Connects to arcvm-boot-notification-server to verify that it is listening.
// When this function is called, the server has just started and may not be
// listening on the socket yet, so this function will retry connecting for up
// to 20s, with exponential backoff. This function can only be called with
// base::MayBlock().
bool IsArcVmBootNotificationServerListening() {
  const base::ElapsedTimer timer;
  const base::TimeDelta limit = g_connect_timeout_limit_for_testing
                                    ? *g_connect_timeout_limit_for_testing
                                    : kConnectTimeoutLimit;
  base::TimeDelta sleep_duration =
      g_connect_sleep_duration_initial_for_testing
          ? *g_connect_sleep_duration_initial_for_testing
          : kConnectSleepDurationInitial;

  do {
    if (ConnectToArcVmBootNotificationServer().is_valid())
      return true;

    LOG(ERROR) << "Retrying to connect to boot notification server in "
               << sleep_duration;
    base::PlatformThread::Sleep(sleep_duration);
    sleep_duration *= 2;
  } while (timer.Elapsed() < limit);
  return false;
}

// Sends upgrade props to arcvm-boot-notification-server over socket at
// |kArcVmBootNotificationServerSocketPath|. This function can only be called
// with base::MayBlock().
bool SendUpgradePropsToArcVmBootNotificationServer(
    const UpgradeParams& params,
    const std::string& serial_number) {
  std::string props = base::JoinString(
      GenerateUpgradeProps(params, serial_number, "ro.boot"), "\n");

  base::ScopedFD fd = ConnectToArcVmBootNotificationServer();
  if (!fd.is_valid())
    return false;

  if (!base::WriteFileDescriptor(fd.get(), props.c_str(), props.size())) {
    // TODO(wvk): Add a unittest to cover this failure once the UpgradeArc flow
    // requires this function to run successfully.
    PLOG(ERROR) << "Unable to write props to "
                << kArcVmBootNotificationServerSocketPath;
    return false;
  }
  return true;
}

// Decodes a job name that may have "_2d" e.g. |kArcCreateDataJobName|
// and returns a decoded string.
std::string DecodeJobName(const std::string& raw_job_name) {
  constexpr const char* kFind = "_2d";
  std::string decoded(raw_job_name);
  base::ReplaceSubstringsAfterOffset(&decoded, 0, kFind, "-");
  return decoded;
}

enum class UpstartOperation {
  JOB_START = 0,
  JOB_STOP,
  // This sends STOP D-Bus message, then sends START. Unlike 'initctl restart',
  // this starts the job even when the job hasn't been started yet (and
  // therefore the stop operation fails.)
  JOB_STOP_AND_START,
};

struct JobDesc {
  std::string job_name;
  UpstartOperation operation;
  std::vector<std::string> environment;
};

void OnConfigureUpstartJobs(std::deque<JobDesc> jobs,
                            chromeos::VoidDBusMethodCallback callback,
                            bool result);

// Starts or stops a job in |jobs| one by one. If starting a job fails, the
// whole operation is aborted and the |callback| is immediately called with
// false. Errors on stopping a job is just ignored with some logs. Once all jobs
// are successfully processed, |callback| is called with true.
void ConfigureUpstartJobs(std::deque<JobDesc> jobs,
                          chromeos::VoidDBusMethodCallback callback) {
  if (jobs.empty()) {
    std::move(callback).Run(true);
    return;
  }

  if (jobs.front().operation == UpstartOperation::JOB_STOP_AND_START) {
    // Expand the restart operation into two, stop and start.
    jobs.front().operation = UpstartOperation::JOB_START;
    jobs.push_front({jobs.front().job_name, UpstartOperation::JOB_STOP,
                     jobs.front().environment});
  }

  const auto& job_name = jobs.front().job_name;
  const auto& operation = jobs.front().operation;
  const auto& environment = jobs.front().environment;

  VLOG(1) << (operation == UpstartOperation::JOB_START ? "Starting "
                                                       : "Stopping ")
          << DecodeJobName(job_name);

  auto wrapped_callback = base::BindOnce(&OnConfigureUpstartJobs,
                                         std::move(jobs), std::move(callback));
  switch (operation) {
    case UpstartOperation::JOB_START:
      chromeos::UpstartClient::Get()->StartJob(job_name, environment,
                                               std::move(wrapped_callback));
      break;
    case UpstartOperation::JOB_STOP:
      chromeos::UpstartClient::Get()->StopJob(job_name, environment,
                                              std::move(wrapped_callback));
      break;
    case UpstartOperation::JOB_STOP_AND_START:
      NOTREACHED();
      break;
  }
}

// Called when the Upstart operation started in ConfigureUpstartJobs is
// done. Handles the fatal error (if any) and then starts the next job.
void OnConfigureUpstartJobs(std::deque<JobDesc> jobs,
                            chromeos::VoidDBusMethodCallback callback,
                            bool result) {
  const std::string job_name = DecodeJobName(jobs.front().job_name);
  const bool is_start = (jobs.front().operation == UpstartOperation::JOB_START);

  if (!result && is_start) {
    LOG(ERROR) << "Failed to start " << job_name;
    // TODO(yusukes): Record UMA for this case.
    std::move(callback).Run(false);
    return;
  }

  VLOG(1) << job_name
          << (is_start ? " started" : (result ? " stopped " : " not running?"));
  jobs.pop_front();
  ConfigureUpstartJobs(std::move(jobs), std::move(callback));
}

// Returns true if the daemon for adb-over-usb should be started on the device.
bool ShouldStartAdbd(bool is_dev_mode,
                     bool is_host_on_vm,
                     bool has_adbd_json,
                     bool is_adb_over_usb_disabled) {
  // Do the same check as ArcSetup::MaybeStartAdbdProxy().
  return is_dev_mode && !is_host_on_vm && has_adbd_json &&
         !is_adb_over_usb_disabled;
}

}  // namespace

class ArcVmClientAdapter : public ArcClientAdapter,
                           public chromeos::ConciergeClient::VmObserver,
                           public chromeos::ConciergeClient::Observer {
 public:
  // Initializing |is_host_on_vm_| and |is_dev_mode_| is not always very fast.
  // Try to initialize them in the constructor and in StartMiniArc respectively.
  // They usually run when the system is not busy.
  ArcVmClientAdapter() : ArcVmClientAdapter(FileSystemStatusRewriter{}) {}

  // For testing purposes and the internal use (by the other ctor) only.
  explicit ArcVmClientAdapter(const FileSystemStatusRewriter& rewriter)
      : is_host_on_vm_(chromeos::system::StatisticsProvider::GetInstance()
                           ->IsRunningOnVm()),
        file_system_status_rewriter_for_testing_(rewriter) {
    auto* client = GetConciergeClient();
    client->AddVmObserver(this);
    client->AddObserver(this);
  }

  ~ArcVmClientAdapter() override {
    auto* client = GetConciergeClient();
    client->RemoveObserver(this);
    client->RemoveVmObserver(this);
  }

  // chromeos::ConciergeClient::VmObserver overrides:
  void OnVmStarted(
      const vm_tools::concierge::VmStartedSignal& signal) override {
    if (signal.name() == kArcVmName)
      VLOG(1) << "OnVmStarted: ARCVM cid=" << signal.vm_info().cid();
  }

  void OnVmStopped(
      const vm_tools::concierge::VmStoppedSignal& signal) override {
    if (signal.name() != kArcVmName)
      return;
    const int64_t cid = signal.cid();
    if (cid != current_cid_) {
      VLOG(1) << "Ignoring VmStopped signal: current CID=" << current_cid_
              << ", stopped CID=" << cid;
      return;
    }
    VLOG(1) << "OnVmStopped: ARCVM cid=" << cid;
    current_cid_ = kInvalidCid;
    OnArcInstanceStopped();
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(StartParams params,
                    chromeos::VoidDBusMethodCallback callback) override {
    // TODO(wvk): Support mini ARC.
    VLOG(2) << "Mini ARCVM instance is not supported.";

    // Save the parameters for the later call to UpgradeArc.
    start_params_ = std::move(params);

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            []() { return GetSystemPropertyInt("cros_debug") == 1; }),
        base::BindOnce(&ArcVmClientAdapter::OnIsDevMode,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopArcInstance(bool on_shutdown, bool should_backup_log) override {
    if (on_shutdown) {
      // Do nothing when |on_shutdown| is true because either vm_concierge.conf
      // job (in case of user session termination) or session_manager (in case
      // of browser-initiated exit on e.g. chrome://flags or UI language change)
      // will stop all VMs including ARCVM right after the browser exits.
      VLOG(1)
          << "StopArcInstance is called during browser shutdown. Do nothing.";
      return;
    }

    if (should_backup_log) {
      GetDebugDaemonClient()->BackupArcBugReport(
          cryptohome::CreateAccountIdentifierFromIdentification(cryptohome_id_),
          base::BindOnce(&ArcVmClientAdapter::OnArcBugReportBackedUp,
                         weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
    } else {
      StopArcInstanceInternal();
    }
  }

  void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                   const std::string& hash,
                   const std::string& serial_number) override {
    DCHECK(cryptohome_id_.id().empty());
    DCHECK(user_id_hash_.empty());
    DCHECK(serial_number_.empty());
    if (cryptohome_id.id().empty())
      LOG(WARNING) << "cryptohome_id is empty";
    if (hash.empty())
      LOG(WARNING) << "hash is empty";
    if (serial_number.empty())
      LOG(WARNING) << "serial_number is empty";
    cryptohome_id_ = cryptohome_id;
    user_id_hash_ = hash;
    serial_number_ = serial_number;
  }

  // chromeos::ConciergeClient::Observer overrides:
  void ConciergeServiceStopped() override {
    VLOG(1) << "vm_concierge stopped";
    // At this point, all crosvm processes are gone. Notify the observer of the
    // event.
    OnArcInstanceStopped();
  }

  void ConciergeServiceStarted() override {}

 private:
  void OnArcBugReportBackedUp(base::TimeTicks arc_bug_report_backup_time,
                              bool result) {
    if (result) {
      base::TimeDelta elapsed_time =
          base::TimeTicks::Now() - arc_bug_report_backup_time;
      base::UmaHistogramCustomTimes(kArcBugReportBackupTimeMetric, elapsed_time,
                                    kArcBugReportBackupTimeMetricMinTime,
                                    kArcBugReportBackupTimeMetricMaxTime,
                                    kArcBugReportBackupTimeMetricBuckets);
    } else {
      LOG(ERROR) << "Error contacting debugd to back up ARC bug report.";
    }

    StopArcInstanceInternal();
  }

  void StopArcInstanceInternal() {
    VLOG(1) << "Stopping arcvm";
    vm_tools::concierge::StopVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    GetConciergeClient()->StopVm(
        request, base::BindOnce(&ArcVmClientAdapter::OnStopVmReply,
                                weak_factory_.GetWeakPtr()));
  }

  void OnIsDevMode(chromeos::VoidDBusMethodCallback callback,
                   bool is_dev_mode) {
    is_dev_mode_ = is_dev_mode;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce([]() {
          std::string output;
          return base::GetAppOutput({"crossystem", "dev_enable_udc?0"},
                                    &output);
        }),
        base::BindOnce(&ArcVmClientAdapter::OnIsAdbOverUsbDisabled,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnIsAdbOverUsbDisabled(chromeos::VoidDBusMethodCallback callback,
                              bool is_adb_over_usb_disabled) {
    is_adb_over_usb_disabled_ = is_adb_over_usb_disabled;
    std::deque<JobDesc> jobs{
        // Note: the first Upstart job is a task, and the callback for the start
        // request won't be called until the task finishes. When the callback is
        // called with true, it is ensured that the per-board features files
        // exist.
        JobDesc{kArcVmPerBoardFeaturesJobName, UpstartOperation::JOB_START, {}},

        JobDesc{kArcVmServerProxyJobName, UpstartOperation::JOB_STOP, {}},
        JobDesc{kArcVmMountMyFilesJobName, UpstartOperation::JOB_STOP, {}},
        JobDesc{
            kArcVmMountRemovableMediaJobName, UpstartOperation::JOB_STOP, {}},
        JobDesc{kArcKeymasterJobName, UpstartOperation::JOB_STOP_AND_START, {}},
        JobDesc{
            kArcSensorServiceJobName, UpstartOperation::JOB_STOP_AND_START, {}},
        JobDesc{kArcHostClockServiceJobName,
                UpstartOperation::JOB_STOP_AND_START,
                {}},
        JobDesc{kArcVmBootNotificationServerJobName,
                UpstartOperation::JOB_STOP_AND_START,
                {}},
    };
    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(
            &ArcVmClientAdapter::OnConfigureUpstartJobsOnStartMiniArc,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnConfigureUpstartJobsOnStartMiniArc(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "ConfigureUpstartJobs (on starting mini ARCVM) failed";
      std::move(callback).Run(false);
      return;
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&IsArcVmBootNotificationServerListening),
        base::BindOnce(
            &ArcVmClientAdapter::OnArcVmBootNotificationServerIsListening,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnArcVmBootNotificationServerIsListening(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to connect to arcvm-boot-notification-server";
      std::move(callback).Run(false);
      return;
    }
    std::move(callback).Run(true);
    // StartMiniArc() successful. Update the member variable here.
    should_notify_observers_ = true;
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    VLOG(2) << "Checking file system status";
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&FileSystemStatus::GetFileSystemStatusBlocking),
        base::BindOnce(&ArcVmClientAdapter::OnFileSystemStatus,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  void OnFileSystemStatus(UpgradeParams params,
                          chromeos::VoidDBusMethodCallback callback,
                          FileSystemStatus file_system_status) {
    VLOG(2) << "Got file system status";
    if (file_system_status_rewriter_for_testing_)
      file_system_status_rewriter_for_testing_.Run(&file_system_status);

    if (user_id_hash_.empty()) {
      LOG(ERROR) << "User ID hash is not set";
      std::move(callback).Run(false);
      return;
    }
    if (serial_number_.empty()) {
      LOG(ERROR) << "Serial number is not set";
      std::move(callback).Run(false);
      return;
    }

    std::vector<std::string> environment_for_create_data = {
        "CHROMEOS_USER=" +
        cryptohome::CreateAccountIdentifierFromIdentification(cryptohome_id_)
            .account_id()};
    std::vector<std::string> environment_for_arcvm_mount_myfiles = {
        "CHROMEOS_USER_ID_HASH=" + user_id_hash_};
    std::deque<JobDesc> jobs{
        JobDesc{kArcVmServerProxyJobName, UpstartOperation::JOB_START, {}},
        JobDesc{kArcCreateDataJobName, UpstartOperation::JOB_START,
                std::move(environment_for_create_data)},
        JobDesc{kArcVmMountMyFilesJobName, UpstartOperation::JOB_START,
                std::move(environment_for_arcvm_mount_myfiles)},
        JobDesc{
            kArcVmMountRemovableMediaJobName, UpstartOperation::JOB_START, {}},
    };
    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(&ArcVmClientAdapter::OnConfigureUpstartJobsOnUpgradeArc,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(file_system_status), std::move(callback)));
  }

  void OnConfigureUpstartJobsOnUpgradeArc(
      UpgradeParams params,
      FileSystemStatus file_system_status,
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "ConfigureUpstartJobs (on upgrading ARCVM) failed";
      std::move(callback).Run(false);
      return;
    }

    const int32_t cpus =
        base::SysInfo::NumberOfProcessors() - start_params_.num_cores_disabled;
    DCHECK_LT(0, cpus);

    DCHECK(is_dev_mode_);
    std::vector<std::string> kernel_cmdline = GenerateKernelCmdline(
        start_params_, params, file_system_status, *is_dev_mode_,
        is_host_on_vm_, GetChromeOsChannelFromLsbRelease(), serial_number_);
    auto start_request = CreateStartArcVmRequest(
        user_id_hash_, cpus, params.demo_session_apps_path, file_system_status,
        std::move(kernel_cmdline));

    const bool should_start_adbd =
        ShouldStartAdbd(*is_dev_mode_, is_host_on_vm_,
                        file_system_status.has_adbd_json(),
                        *is_adb_over_usb_disabled_) ||
        g_enable_adb_over_usb_for_testing;
    GetConciergeClient()->StartArcVm(
        start_request, base::BindOnce(&ArcVmClientAdapter::OnStartArcVmReply,
                                      weak_factory_.GetWeakPtr(),
                                      should_start_adbd, std::move(callback)));

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&SendUpgradePropsToArcVmBootNotificationServer, params,
                       serial_number_),
        base::BindOnce([](bool result) {
          VLOG(1)
              << "Sending upgrade props to arcvm-boot-notification-server was "
              << (result ? "successful" : "unsuccessful");
        }));
  }

  void OnStartArcVmReply(
      bool should_start_adbd,
      chromeos::VoidDBusMethodCallback callback,
      base::Optional<vm_tools::concierge::StartVmResponse> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to start arcvm. Empty response.";
      std::move(callback).Run(false);
      return;
    }

    const vm_tools::concierge::StartVmResponse& response = reply.value();
    if (response.status() != vm_tools::concierge::VM_STATUS_RUNNING) {
      LOG(ERROR) << "Failed to start arcvm: status=" << response.status()
                 << ", reason=" << response.failure_reason();
      std::move(callback).Run(false);
      return;
    }
    current_cid_ = response.vm_info().cid();
    VLOG(1) << "ARCVM started cid=" << current_cid_;

    if (!should_start_adbd) {
      // No need to start arcvm-adbd. Run the |callback| now.
      std::move(callback).Run(true);
      return;
    }

    // Start the daemon for supporting adb-over-usb.
    VLOG(1) << "Starting arcvm-adbd";
    std::vector<std::string> environment_for_adbd = {
        "SERIALNUMBER=" + serial_number_,
        base::StringPrintf("ARCVM_CID=%" PRId64, current_cid_)};
    std::deque<JobDesc> jobs{JobDesc{kArcVmAdbdJobName,
                                     UpstartOperation::JOB_STOP_AND_START,
                                     std::move(environment_for_adbd)}};
    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(&ArcVmClientAdapter::OnConfigureUpstartJobsAfterVmStart,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnConfigureUpstartJobsAfterVmStart(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "ConfigureUpstartJobs (after starting ARCVM) failed. "
                    "Stopping the VM..";
      StopArcInstanceInternal();
    }
    std::move(callback).Run(result);
  }

  void OnArcInstanceStopped() {
    VLOG(1) << "ARCVM stopped.";

    // If this method is called before even mini VM is started (e.g. very early
    // vm_concierge crash), or this method is called twice (e.g. crosvm crash
    // followed by vm_concierge crash), do nothing.
    if (!should_notify_observers_)
      return;
    should_notify_observers_ = false;

    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

  void OnStopVmReply(
      base::Optional<vm_tools::concierge::StopVmResponse> reply) {
    // If the reply indicates the D-Bus call is successfully done, do nothing.
    // Concierge will call OnVmStopped() eventually.
    if (reply.has_value() && reply.value().success())
      return;

    // We likely tried to stop mini VM which doesn't exist today. Notify
    // observers.
    // TODO(wvk): Remove the fallback once we implement mini VM.
    OnArcInstanceStopped();
  }

  base::Optional<bool> is_dev_mode_;
  // True when the *host* is running on a VM.
  const bool is_host_on_vm_;
  // True when adb-over-usb is disabled.
  base::Optional<bool> is_adb_over_usb_disabled_;

  // A cryptohome ID of the primary profile.
  cryptohome::Identification cryptohome_id_;
  // A hash of the primary profile user ID.
  std::string user_id_hash_;
  // A serial number for the current profile.
  std::string serial_number_;

  StartParams start_params_;
  bool should_notify_observers_ = false;
  int64_t current_cid_ = kInvalidCid;

  FileSystemStatusRewriter file_system_status_rewriter_for_testing_;

  // For callbacks.
  base::WeakPtrFactory<ArcVmClientAdapter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcVmClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter() {
  return std::make_unique<ArcVmClientAdapter>();
}

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapterForTesting(
    const FileSystemStatusRewriter& rewriter) {
  return std::make_unique<ArcVmClientAdapter>(rewriter);
}

void SetArcVmBootNotificationServerAddressForTesting(
    const std::string& new_address,
    base::TimeDelta connect_timeout_limit,
    base::TimeDelta connect_sleep_duration_initial) {
  sockaddr_un* address =
      const_cast<sockaddr_un*>(GetArcVmBootNotificationServerAddress());
  DCHECK_GE(sizeof(address->sun_path), new_address.size());
  DCHECK_GT(connect_timeout_limit, connect_sleep_duration_initial);

  memset(address->sun_path, 0, sizeof(address->sun_path));
  // |new_address| may contain '\0' if it is an abstract socket address, so use
  // memcpy instead of strcpy.
  memcpy(address->sun_path, new_address.data(), new_address.size());

  g_connect_timeout_limit_for_testing = connect_timeout_limit;
  g_connect_sleep_duration_initial_for_testing = connect_sleep_duration_initial;
}

void EnableAdbOverUsbForTesting() {
  g_enable_adb_over_usb_for_testing = true;
}

std::vector<std::string> GenerateUpgradeProps(
    const UpgradeParams& upgrade_params,
    const std::string& serial_number,
    const std::string& prefix) {
  std::vector<std::string> result = {
      base::StringPrintf("%s.disable_boot_completed=%d", prefix.c_str(),
                         upgrade_params.skip_boot_completed_broadcast),
      base::StringPrintf("%s.copy_packages_cache=%d", prefix.c_str(),
                         static_cast<int>(upgrade_params.packages_cache_mode)),
      base::StringPrintf("%s.skip_gms_core_cache=%d", prefix.c_str(),
                         upgrade_params.skip_gms_core_cache),
      base::StringPrintf("%s.arc_demo_mode=%d", prefix.c_str(),
                         upgrade_params.is_demo_session),
      base::StringPrintf(
          "%s.supervision.transition=%d", prefix.c_str(),
          static_cast<int>(upgrade_params.supervision_transition)),
      base::StringPrintf("%s.serialno=%s", prefix.c_str(),
                         serial_number.c_str()),
  };
  // Conditionally sets more properties based on |upgrade_params|.
  if (!upgrade_params.locale.empty()) {
    result.push_back(base::StringPrintf("%s.locale=%s", prefix.c_str(),
                                        upgrade_params.locale.c_str()));
    if (!upgrade_params.preferred_languages.empty()) {
      result.push_back(base::StringPrintf(
          "%s.preferred_languages=%s", prefix.c_str(),
          base::JoinString(upgrade_params.preferred_languages, ",").c_str()));
    }
  }

  // TODO(niwa): Handle |is_account_managed| and
  // |is_managed_adb_sideloading_allowed| in |upgrade_params| when we
  // implement apk sideloading for ARCVM.
  return result;
}

}  // namespace arc
