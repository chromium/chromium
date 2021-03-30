// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_vm_client_adapter.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <deque>
#include <set>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
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
#include "chromeos/components/sensors/buildflags.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
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
constexpr const char kArcVmPerBoardFeaturesJobName[] =
    "arcvm_2dper_2dboard_2dfeatures";
constexpr char kArcVmPreLoginServicesJobName[] =
    "arcvm_2dpre_2dlogin_2dservices";
constexpr char kArcVmPostLoginServicesJobName[] =
    "arcvm_2dpost_2dlogin_2dservices";
constexpr char kArcVmPostVmStartServicesJobName[] =
    "arcvm_2dpost_2dvm_2dstart_2dservices";

constexpr const char kArcVmBootNotificationServerSocketPath[] =
    "/run/arcvm_boot_notification_server/host.socket";

constexpr base::TimeDelta kArcBugReportBackupTimeMetricMinTime =
    base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kArcBugReportBackupTimeMetricMaxTime =
    base::TimeDelta::FromSeconds(60);
constexpr int kArcBugReportBackupTimeMetricBuckets = 50;
constexpr const char kArcBugReportBackupTimeMetric[] =
    "Login.ArcBugReportBackupTime";

// The owner ID that ARCVM is started with for mini-ARCVM. On UpgradeArc,
// the owner ID is set to the logged-in user.
constexpr const char kArcVmDefaultOwner[] = "ARCVM_DEFAULT_OWNER";

constexpr int64_t kInvalidCid = -1;

constexpr base::TimeDelta kConnectTimeoutLimit =
    base::TimeDelta::FromSeconds(20);
constexpr base::TimeDelta kConnectSleepDurationInitial =
    base::TimeDelta::FromMilliseconds(100);

base::Optional<base::TimeDelta> g_connect_timeout_limit_for_testing;
base::Optional<base::TimeDelta> g_connect_sleep_duration_initial_for_testing;
base::Optional<int> g_boot_notification_server_fd;

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

std::vector<std::string> GenerateUpgradeProps(
    const UpgradeParams& upgrade_params,
    const std::string& serial_number,
    const std::string& prefix) {
  std::vector<std::string> result = {
      base::StringPrintf("%s.disable_boot_completed=%d", prefix.c_str(),
                         upgrade_params.skip_boot_completed_broadcast),
      base::StringPrintf("%s.enable_adb_sideloading=%d", prefix.c_str(),
                         upgrade_params.is_adb_sideloading_enabled),
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

std::vector<std::string> GenerateKernelCmdline(
    const StartParams& start_params,
    const FileSystemStatus& file_system_status,
    bool is_dev_mode,
    bool is_host_on_vm,
    const std::string& channel) {
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
      base::StringPrintf("androidboot.iioservice_present=%d",
                         BUILDFLAG(USE_IIOSERVICE)),
  };

  ArcVmUreadaheadMode mode = GetArcVmUreadaheadMode();
  switch (mode) {
    case ArcVmUreadaheadMode::READAHEAD:
      result.push_back("androidboot.arcvm_ureadahead_mode=readahead");
      break;
    case ArcVmUreadaheadMode::GENERATE:
      result.push_back("androidboot.arcvm_ureadahead_mode=generate");
      break;
    case ArcVmUreadaheadMode::DISABLED:
      break;
  }

  // We run vshd under a restricted domain on non-test images.
  // (go/arcvm-android-sh-restricted)
  if (channel == "testimage")
    result.push_back("androidboot.vshd_service_override=vshd_for_test");

  // TODO(niwa): Check if we need to set ro.boot.enable_adb_sideloading for
  // ARCVM.

  // Only add boot property if flag to disable media store maintenance is set.
  if (start_params.disable_media_store_maintenance)
    result.push_back("androidboot.disable_media_store_maintenance=1");

  if (start_params.arc_generate_play_auto_install)
    result.push_back("androidboot.arc_generate_pai=1");

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

  // Check if enabled.
  if (base::FeatureList::IsEnabled(arc::kUseHighMemoryDalvikProfile)) {
    switch (start_params.dalvik_memory_profile) {
      case StartParams::DalvikMemoryProfile::DEFAULT:
        break;
      case StartParams::DalvikMemoryProfile::M4G:
        result.push_back("androidboot.arc_dalvik_memory_profile=4G");
        break;
      case StartParams::DalvikMemoryProfile::M8G:
        result.push_back("androidboot.arc_dalvik_memory_profile=8G");
        break;
      case StartParams::DalvikMemoryProfile::M16G:
        result.push_back("androidboot.arc_dalvik_memory_profile=16G");
        break;
    }
  } else {
    VLOG(1) << "High-memory dalvik profile is not enabled, default low-memory "
               "is used.";
  }

  std::string log_profile_name;
  switch (start_params.usap_profile) {
    case StartParams::UsapProfile::DEFAULT:
      log_profile_name = "default low-memory";
      break;
    case StartParams::UsapProfile::M4G:
      result.push_back("androidboot.usap_profile=4G");
      log_profile_name = "high-memory 4G";
      break;
    case StartParams::UsapProfile::M8G:
      result.push_back("androidboot.usap_profile=8G");
      log_profile_name = "high-memory 8G";
      break;
    case StartParams::UsapProfile::M16G:
      result.push_back("androidboot.usap_profile=16G");
      log_profile_name = "high-memory 16G";
      break;
  }
  VLOG(1) << "Applied " << log_profile_name << " USAP profile";

  if (start_params.disable_download_provider)
    result.push_back("androidboot.disable_download_provider=1");

  return result;
}

vm_tools::concierge::StartArcVmRequest CreateStartArcVmRequest(
    uint32_t cpus,
    const base::FilePath& demo_session_apps_path,
    const FileSystemStatus& file_system_status,
    std::vector<std::string> kernel_cmdline) {
  vm_tools::concierge::StartArcVmRequest request;

  request.set_name(kArcVmName);
  request.set_owner_id(kArcVmDefaultOwner);

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
  // the one in GenerateFirstStageFstab() in platform2/arc/setup/.
  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(file_system_status.vendor_image_path().value());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);

  // Add /run/imageloader/.../android_demo_apps.squash as /dev/block/vdc if
  // needed.
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

  // Add enable_rt_vcpu.
  request.set_enable_rt_vcpu(IsArcVmRtVcpuEnabled(cpus));
  return request;
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
  if (g_boot_notification_server_fd)
    return base::ScopedFD(HANDLE_EINTR(dup(*g_boot_notification_server_fd)));

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
    PLOG(ERROR) << "Unable to write props to "
                << kArcVmBootNotificationServerSocketPath;
    return false;
  }
  return true;
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
    start_params_ = std::move(params);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(
            []() { return GetSystemPropertyInt("cros_debug") == 1; }),
        base::BindOnce(&ArcVmClientAdapter::OnIsDevMode,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    if (user_id_hash_.empty()) {
      LOG(ERROR) << "User ID hash is not set";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }
    if (serial_number_.empty()) {
      LOG(ERROR) << "Serial number is not set";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    // Stop the existing full-VM if any (e.g. in case of a chrome crash).
    VLOG(1) << "Stopping the existing full-VM if any.";
    vm_tools::concierge::StopVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    GetConciergeClient()->StopVm(
        request, base::BindOnce(&ArcVmClientAdapter::OnExistingFullVmStopped,
                                weak_factory_.GetWeakPtr(), std::move(params),
                                std::move(callback)));
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
    DCHECK_NE(current_cid_, kInvalidCid) << "ARCVM is not running.";

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

  void SetDemoModeDelegate(DemoModeDelegate* delegate) override {
    demo_mode_delegate_ = delegate;
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
    // This may be called before ARCVM has been upgraded and the proper VM id
    // has been set. Since ConciergeClient::StopVm() returns successfully
    // regardless of whether the VM exists, check to see which VM is actually
    // running.

    vm_tools::concierge::GetVmInfoRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    GetConciergeClient()->GetVmInfo(
        request, base::BindOnce(&ArcVmClientAdapter::OnGetVmReply,
                                weak_factory_.GetWeakPtr()));
  }

  void OnGetVmReply(
      base::Optional<vm_tools::concierge::GetVmInfoResponse> reply) {
    vm_tools::concierge::StopVmRequest request;
    request.set_name(kArcVmName);

    if (reply.has_value() && reply.value().success())
      request.set_owner_id(user_id_hash_);
    else
      request.set_owner_id(kArcVmDefaultOwner);

    GetConciergeClient()->StopVm(
        request, base::BindOnce(&ArcVmClientAdapter::OnStopVmReply,
                                weak_factory_.GetWeakPtr()));
  }

  void OnIsDevMode(chromeos::VoidDBusMethodCallback callback,
                   bool is_dev_mode) {
    is_dev_mode_ = is_dev_mode;
    std::deque<JobDesc> jobs{
        // Note: the first Upstart job is a task, and the callback for the start
        // request won't be called until the task finishes. When the callback is
        // called with true, it is ensured that the per-board features files
        // exist.
        JobDesc{kArcVmPerBoardFeaturesJobName, UpstartOperation::JOB_START, {}},
        JobDesc{
            kArcVmPostVmStartServicesJobName, UpstartOperation::JOB_STOP, {}},
        JobDesc{kArcVmPostLoginServicesJobName, UpstartOperation::JOB_STOP, {}},
        JobDesc{kArcVmPreLoginServicesJobName,
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

    VLOG(1) << "Waiting for Concierge to be available";
    GetConciergeClient()->WaitForServiceToBeAvailable(
        base::BindOnce(&ArcVmClientAdapter::OnConciergeAvailable,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnConciergeAvailable(chromeos::VoidDBusMethodCallback callback,
                            bool service_available) {
    if (!service_available) {
      LOG(ERROR) << "Failed to wait for Concierge to be available";
      std::move(callback).Run(false);
      return;
    }

    // Stop the existing mini-VM if any (e.g. in case of a chrome crash).
    VLOG(1) << "Stopping the existing mini-VM if any.";
    vm_tools::concierge::StopVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(kArcVmDefaultOwner);
    GetConciergeClient()->StopVm(
        request,
        base::BindOnce(&ArcVmClientAdapter::OnExistingMiniVmStopped,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnExistingMiniVmStopped(
      chromeos::VoidDBusMethodCallback callback,
      base::Optional<vm_tools::concierge::StopVmResponse> reply) {
    // reply->success() returns true even when there was no VM running.
    if (!reply.has_value() || !reply->success()) {
      LOG(ERROR) << "StopVm failed: "
                 << (reply.has_value() ? reply->failure_reason()
                                       : "No D-Bus response.");
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

    VLOG(2) << "Checking file system status";
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&FileSystemStatus::GetFileSystemStatusBlocking),
        base::BindOnce(&ArcVmClientAdapter::OnFileSystemStatus,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnFileSystemStatus(chromeos::VoidDBusMethodCallback callback,
                          FileSystemStatus file_system_status) {
    VLOG(2) << "Got file system status";
    if (file_system_status_rewriter_for_testing_)
      file_system_status_rewriter_for_testing_.Run(&file_system_status);

    VLOG(2) << "Retrieving demo session apps path";
    DCHECK(demo_mode_delegate_);
    demo_mode_delegate_->EnsureOfflineResourcesLoaded(base::BindOnce(
        &ArcVmClientAdapter::OnDemoResourcesLoaded, weak_factory_.GetWeakPtr(),
        std::move(callback), std::move(file_system_status)));
  }

  void OnDemoResourcesLoaded(chromeos::VoidDBusMethodCallback callback,
                             FileSystemStatus file_system_status) {
    const base::FilePath demo_session_apps_path =
        demo_mode_delegate_->GetDemoAppsPath();

    const int32_t cpus =
        base::SysInfo::NumberOfProcessors() - start_params_.num_cores_disabled;
    DCHECK_LT(0, cpus);

    DCHECK(is_dev_mode_);
    std::vector<std::string> kernel_cmdline = GenerateKernelCmdline(
        start_params_, file_system_status, *is_dev_mode_, is_host_on_vm_,
        GetChromeOsChannelFromLsbRelease());
    auto start_request =
        CreateStartArcVmRequest(cpus, demo_session_apps_path,
                                file_system_status, std::move(kernel_cmdline));

    GetConciergeClient()->StartArcVm(
        start_request,
        base::BindOnce(&ArcVmClientAdapter::OnStartArcVmReply,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnStartArcVmReply(
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
    should_notify_observers_ = true;
    VLOG(1) << "ARCVM started cid=" << current_cid_;
    std::move(callback).Run(true);
  }

  void OnExistingFullVmStopped(
      UpgradeParams params,
      chromeos::VoidDBusMethodCallback callback,
      base::Optional<vm_tools::concierge::StopVmResponse> reply) {
    // reply->success() returns true even when there was no VM running.
    if (!reply.has_value() || !reply->success()) {
      LOG(ERROR) << "StopVm failed: "
                 << (reply.has_value() ? reply->failure_reason()
                                       : "No D-Bus response.");
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    VLOG(1) << "Checking adb sideload status";
    chromeos::SessionManagerClient::Get()->QueryAdbSideload(base::BindOnce(
        &ArcVmClientAdapter::OnQueryAdbSideload, weak_factory_.GetWeakPtr(),
        std::move(params), std::move(callback)));
  }

  void OnQueryAdbSideload(
      UpgradeParams params,
      chromeos::VoidDBusMethodCallback callback,
      chromeos::SessionManagerClient::AdbSideloadResponseCode response_code,
      bool enabled) {
    VLOG(1) << "IsAdbSideloadAllowed, response_code="
            << static_cast<int>(response_code) << ", enabled=" << enabled;

    switch (response_code) {
      case chromeos::SessionManagerClient::AdbSideloadResponseCode::FAILED:
        LOG(ERROR) << "Failed response from QueryAdbSideload";
        StopArcInstanceInternal();
        std::move(callback).Run(false);
        return;
      case chromeos::SessionManagerClient::AdbSideloadResponseCode::
          NEED_POWERWASH:
        params.is_adb_sideloading_enabled = false;
        break;
      case chromeos::SessionManagerClient::AdbSideloadResponseCode::SUCCESS:
        params.is_adb_sideloading_enabled = enabled;
        break;
    }

    VLOG(1) << "Starting upstart jobs for UpgradeArc()";
    std::vector<std::string> environment{
        "CHROMEOS_USER=" +
            cryptohome::CreateAccountIdentifierFromIdentification(
                cryptohome_id_)
                .account_id(),
        "CHROMEOS_USER_ID_HASH=" + user_id_hash_};
    std::deque<JobDesc> jobs{
        JobDesc{kArcVmPostLoginServicesJobName, UpstartOperation::JOB_START,
                std::move(environment)},
    };

    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(&ArcVmClientAdapter::OnConfigureUpstartJobsOnUpgradeArc,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  void OnConfigureUpstartJobsOnUpgradeArc(
      UpgradeParams params,
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "ConfigureUpstartJobs (on upgrading ARCVM) failed. ";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    VLOG(1) << "Setting owner ID for mini-VM instance.";
    vm_tools::concierge::SetVmIdRequest request;
    request.set_name(kArcVmName);
    request.set_src_owner_id(kArcVmDefaultOwner);
    request.set_dest_owner_id(user_id_hash_);
    GetConciergeClient()->SetVmId(
        request, base::BindOnce(&ArcVmClientAdapter::OnSetVmId,
                                weak_factory_.GetWeakPtr(), std::move(params),
                                std::move(callback)));
  }

  void OnSetVmId(UpgradeParams params,
                 chromeos::VoidDBusMethodCallback callback,
                 base::Optional<vm_tools::concierge::SetVmIdResponse> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to set VM ID. Empty response.";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    const vm_tools::concierge::SetVmIdResponse& response = reply.value();
    if (!response.success()) {
      LOG(ERROR) << "Failed to set VM ID. Failure reason="
                 << response.failure_reason();
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    VLOG(2) << "Set VM id for default instance";

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&SendUpgradePropsToArcVmBootNotificationServer,
                       std::move(params), serial_number_),
        base::BindOnce(&ArcVmClientAdapter::OnUpgradePropsSent,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnUpgradePropsSent(chromeos::VoidDBusMethodCallback callback,
                          bool result) {
    if (!result) {
      LOG(ERROR)
          << "Failed to send upgrade props to arcvm-boot-notification-server";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    VLOG(1) << "Starting arcvm-post-vm-start-services.";
    std::vector<std::string> environment;
    std::deque<JobDesc> jobs{JobDesc{kArcVmPostVmStartServicesJobName,
                                     UpstartOperation::JOB_START,
                                     std::move(environment)}};
    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(&ArcVmClientAdapter::OnConfigureUpstartJobsAfterVmStart,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnConfigureUpstartJobsAfterVmStart(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "ConfigureUpstartJobs (after starting ARCVM) failed.";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    VLOG(1) << "ARCVM upgrade completed";
    std::move(callback).Run(true);
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

    // StopVm always returns successfully, so the only case where this happens
    // is if the reply is empty, which means Concierge isn't running and ARCVM
    // isn't either.
    LOG(ERROR) << "Failed to stop ARCVM: empty reply.";
    OnArcInstanceStopped();
  }

  base::Optional<bool> is_dev_mode_;
  // True when the *host* is running on a VM.
  const bool is_host_on_vm_;
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

  // The delegate is owned by ArcSessionRunner.
  DemoModeDelegate* demo_mode_delegate_ = nullptr;

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

void SetArcVmBootNotificationServerFdForTesting(base::Optional<int> fd) {
  g_boot_notification_server_fd = fd;
}

std::vector<std::string> GenerateUpgradePropsForTesting(
    const UpgradeParams& upgrade_params,
    const std::string& serial_number,
    const std::string& prefix) {
  return GenerateUpgradeProps(upgrade_params, serial_number, prefix);
}

}  // namespace arc
