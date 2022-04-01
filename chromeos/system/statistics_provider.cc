// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/statistics_provider.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/system/kiosk_oem_manifest_parser.h"
#include "chromeos/system/name_value_pairs_parser.h"

namespace chromeos {
namespace system {

namespace {

// Path to the tool used to get system info, and special values for the
// output of the tool.
const char* kCrosSystemTool[] = {"/usr/bin/crossystem"};
const char kCrosSystemValueError[] = "(error)";

const char kHardwareClassCrosSystemKey[] = "hwid";
const char kHardwareClassValueUnknown[] = "unknown";

const char kIsVmCrosSystemKey[] = "inside_vm";

// File to get ECHO coupon info from, and key/value delimiters of
// the file.
const char kEchoCouponFile[] =
    "/mnt/stateful_partition/unencrypted/cache/vpd/echo/vpd_echo.txt";

// File to get regional data from.
const char kCrosRegions[] = "/usr/share/misc/cros-regions.json";

// Timeout that we should wait for statistics to get loaded
const int kTimeoutSecs = 3;

// The location of OEM manifest file used to trigger OOBE flow for kiosk mode.
const base::CommandLine::CharType kOemManifestFilePath[] =
    FILE_PATH_LITERAL("/usr/share/oem/oobe/manifest.json");

// Items in region dictionary.
const char kKeyboardsPath[] = "keyboards";
const char kLocalesPath[] = "locales";
const char kTimeZonesPath[] = "time_zones";
const char kKeyboardMechanicalLayoutPath[] = "keyboard_mechanical_layout";

// These are the machine serial number keys that we check in order until we find
// a non-empty serial number.
//
// On older Samsung devices the VPD contains two serial numbers: "Product_S/N"
// and "serial_number" which are based on the same value except that the latter
// has a letter appended that serves as a check digit. Unfortunately, the
// sticker on the device packaging didn't include that check digit (the sticker
// on the device did though!). The former sticker was the source of the serial
// number used by device management service, so we preferred "Product_S/N" over
// "serial_number" to match the server. As an unintended consequence, older
// Samsung devices display and report a serial number that doesn't match the
// sticker on the device (the check digit is missing).
//
// "Product_S/N" is known to be used on celes, lumpy, pi, pit, snow, winky and
// some kevin devices and thus needs to be supported until AUE of these
// devices. It's known *not* to be present on caroline.
// TODO(tnagel): Remove "Product_S/N" after all devices that have it are AUE.
const char* const kMachineInfoSerialNumberKeys[] = {
    "flex_id",        // Used by Reven devices
    "Product_S/N",    // Samsung legacy
    "serial_number",  // VPD v2+ devices (Samsung: caroline and later)
};

// Gets ListValue from given |dictionary| by given |key| and (unless |result| is
// nullptr) sets |result| to a string with all list values joined by ','.
// Returns true on success.
bool JoinListValuesToString(const base::Value& dictionary,
                            const std::string key,
                            std::string* result) {
  const base::Value* list_value = dictionary.FindListKey(key);
  if (list_value == nullptr)
    return false;

  std::string buffer;
  bool first = true;
  for (const auto& v : list_value->GetListDeprecated()) {
    const std::string* value = v.GetIfString();
    if (!value)
      return false;

    if (first)
      first = false;
    else
      buffer += ',';

    buffer += *value;
  }
  if (result != nullptr)
    *result = buffer;
  return true;
}

// Gets ListValue from given |dictionary| by given |key| and (unless |result| is
// nullptr) sets |result| to the first value as string.  Returns true on
// success.
bool GetFirstListValueAsString(const base::Value& dictionary,
                               const std::string key,
                               std::string* result) {
  const base::Value* list_value = dictionary.FindListKey(key);
  if (list_value == nullptr || list_value->GetListDeprecated().empty())
    return false;

  const std::string* value = list_value->GetListDeprecated()[0].GetIfString();
  if (value == nullptr)
    return false;
  if (result != nullptr)
    *result = *value;
  return true;
}

bool GetKeyboardLayoutFromRegionalData(const base::Value& region_dict,
                                       std::string* result) {
  return JoinListValuesToString(region_dict, kKeyboardsPath, result);
}

bool GetKeyboardMechanicalLayoutFromRegionalData(const base::Value& region_dict,
                                                 std::string* result) {
  const std::string* value =
      region_dict.FindStringPath(kKeyboardMechanicalLayoutPath);
  if (value == nullptr)
    return false;
  *result = *value;
  return true;
}

bool GetInitialTimezoneFromRegionalData(const base::Value& region_dict,
                                        std::string* result) {
  return GetFirstListValueAsString(region_dict, kTimeZonesPath, result);
}

bool GetInitialLocaleFromRegionalData(const base::Value& region_dict,
                                      std::string* result) {
  return JoinListValuesToString(region_dict, kLocalesPath, result);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VpdCacheReadResult {
  kSuccess = 0,
  KMissing = 1,
  kParseFailed = 2,
  kMaxValue = kParseFailed,
};

void ReportVpdCacheReadResult(VpdCacheReadResult result) {
  base::UmaHistogramEnumeration("Enterprise.VPDCacheReadResult", result);
}

}  // namespace

// Key values for GetMachineStatistic()/GetMachineFlag() calls.
const char kActivateDateKey[] = "ActivateDate";
const char kBlockDevModeKey[] = "block_devmode";
const char kCheckEnrollmentKey[] = "check_enrollment";
const char kShouldSendRlzPingKey[] = "should_send_rlz_ping";
const char kShouldSendRlzPingValueFalse[] = "0";
const char kShouldSendRlzPingValueTrue[] = "1";
const char kRlzEmbargoEndDateKey[] = "rlz_embargo_end_date";
const char kEnterpriseManagementEmbargoEndDateKey[] =
    "enterprise_management_embargo_end_date";
const char kCustomizationIdKey[] = "customization_id";
const char kDevSwitchBootKey[] = "devsw_boot";
const char kDevSwitchBootValueDev[] = "1";
const char kDevSwitchBootValueVerified[] = "0";
const char kDockMacAddressKey[] = "dock_mac";
const char kEthernetMacAddressKey[] = "ethernet_mac0";
const char kFirmwareWriteProtectCurrentKey[] = "wpsw_cur";
const char kFirmwareWriteProtectCurrentValueOn[] = "1";
const char kFirmwareWriteProtectCurrentValueOff[] = "0";
const char kFirmwareTypeKey[] = "mainfw_type";
const char kFirmwareTypeValueDeveloper[] = "developer";
const char kFirmwareTypeValueNonchrome[] = "nonchrome";
const char kFirmwareTypeValueNormal[] = "normal";
const char kHardwareClassKey[] = "hardware_class";
const char kIsVmKey[] = "is_vm";
const char kIsVmValueFalse[] = "0";
const char kIsVmValueTrue[] = "1";
const char kManufactureDateKey[] = "mfg_date";
const char kOffersCouponCodeKey[] = "ubind_attribute";
const char kOffersGroupCodeKey[] = "gbind_attribute";
const char kRlzBrandCodeKey[] = "rlz_brand_code";
const char kRegionKey[] = "region";
const char kSerialNumberKeyForTest[] = "serial_number";
const char kInitialLocaleKey[] = "initial_locale";
const char kInitialTimezoneKey[] = "initial_timezone";
const char kKeyboardLayoutKey[] = "keyboard_layout";
const char kKeyboardMechanicalLayoutKey[] = "keyboard_mechanical_layout";
const char kAttestedDeviceIdKey[] = "attested_device_id";

// OEM specific statistics. Must be prefixed with "oem_".
const char kOemCanExitEnterpriseEnrollmentKey[] = "oem_can_exit_enrollment";
const char kOemDeviceRequisitionKey[] = "oem_device_requisition";
const char kOemIsEnterpriseManagedKey[] = "oem_enterprise_managed";
const char kOemKeyboardDrivenOobeKey[] = "oem_keyboard_driven_oobe";

bool HasOemPrefix(const std::string& name) {
  return name.substr(0, 4) == "oem_";
}

// The StatisticsProvider implementation used in production.
class StatisticsProviderImpl : public StatisticsProvider {
 public:
  // StatisticsProvider implementation:
  void StartLoadingMachineStatistics(bool load_oem_manifest) override;
  void ScheduleOnMachineStatisticsLoaded(base::OnceClosure callback) override;
  bool GetMachineStatistic(const std::string& name,
                           std::string* result) override;
  bool GetMachineFlag(const std::string& name, bool* result) override;
  void Shutdown() override;

  // Returns true when Chrome OS is running in a VM. NOTE: if crossystem is not
  // installed it will return false even if Chrome OS is running in a VM.
  bool IsRunningOnVm() override;

  static StatisticsProviderImpl* GetInstance();

  StatisticsProviderImpl(const StatisticsProviderImpl&) = delete;
  StatisticsProviderImpl& operator=(const StatisticsProviderImpl&) = delete;

 private:
  typedef std::map<std::string, bool> MachineFlags;
  typedef bool (*RegionDataExtractor)(const base::Value&, std::string*);
  friend struct base::DefaultSingletonTraits<StatisticsProviderImpl>;

  StatisticsProviderImpl();
  ~StatisticsProviderImpl() override;

  // Called when statistics have finished loading. Unblocks pending calls to
  // WaitForStatisticsLoaded() and schedules callbacks passed to
  // ScheduleOnMachineStatisticsLoaded().
  void SignalStatisticsLoaded();

  // Waits up to |kTimeoutSecs| for statistics to be loaded. Returns true if
  // they were loaded successfully.
  bool WaitForStatisticsLoaded();

  // Loads the machine statistics off of disk. Runs on the file thread.
  void LoadMachineStatistics(bool load_oem_manifest);

  // Loads the OEM statistics off of disk. Runs on the file thread.
  void LoadOemManifestFromFile(const base::FilePath& file);

  // Loads regional data off of disk. Runs on the file thread.
  void LoadRegionsFile(const base::FilePath& filename);

  // Extracts known data from regional_data_;
  // Returns true on success;
  bool GetRegionalInformation(const std::string& name,
                              std::string* result) const;

  // Returns extractor from regional_data_extractors_ or nullptr.
  RegionDataExtractor GetRegionalDataExtractor(const std::string& name) const;

  bool load_statistics_started_;
  NameValuePairsParser::NameValueMap machine_info_;
  MachineFlags machine_flags_;
  base::AtomicFlag cancellation_flag_;
  bool oem_manifest_loaded_;
  std::string region_;
  base::Value region_dict_;
  base::flat_map<std::string, RegionDataExtractor> regional_data_extractors_;

  // Lock held when |statistics_loaded_| is signaled and when
  // |statistics_loaded_callbacks_| is accessed.
  base::Lock statistics_loaded_lock_;

  // Signaled once machine statistics are loaded. It is guaranteed that
  // |machine_info_| and |machine_flags_| don't change once this is signaled.
  base::WaitableEvent statistics_loaded_;

  // Callbacks to schedule once machine statistics are loaded.
  std::vector<
      std::pair<base::OnceClosure, scoped_refptr<base::SequencedTaskRunner>>>
      statistics_loaded_callbacks_;
};

void StatisticsProviderImpl::SignalStatisticsLoaded() {
  decltype(statistics_loaded_callbacks_) local_statistics_loaded_callbacks;

  {
    base::AutoLock auto_lock(statistics_loaded_lock_);

    // Move all callbacks to a local variable.
    local_statistics_loaded_callbacks = std::move(statistics_loaded_callbacks_);

    // Prevent new callbacks from being added to |statistics_loaded_callbacks_|
    // and unblock pending WaitForStatisticsLoaded() calls.
    statistics_loaded_.Signal();

    VLOG(1) << "Finished loading statistics.";
  }

  // Schedule callbacks that were in |statistics_loaded_callbacks_|.
  for (auto& callback : local_statistics_loaded_callbacks)
    callback.second->PostTask(FROM_HERE, std::move(callback.first));
}

bool StatisticsProviderImpl::WaitForStatisticsLoaded() {
  CHECK(load_statistics_started_);
  if (statistics_loaded_.IsSignaled())
    return true;

  // Block if the statistics are not loaded yet. Normally this shouldn't
  // happen except during OOBE.
  base::Time start_time = base::Time::Now();
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  statistics_loaded_.TimedWait(base::Seconds(kTimeoutSecs));

  base::TimeDelta dtime = base::Time::Now() - start_time;
  if (statistics_loaded_.IsSignaled()) {
    VLOG(1) << "Statistics loaded after waiting " << dtime.InMilliseconds()
            << "ms.";
    return true;
  }

  LOG(ERROR) << "Statistics not loaded after waiting " << dtime.InMilliseconds()
             << "ms.";
  return false;
}

StatisticsProviderImpl::RegionDataExtractor
StatisticsProviderImpl::GetRegionalDataExtractor(
    const std::string& name) const {
  const auto it = regional_data_extractors_.find(name);
  if (it == regional_data_extractors_.end())
    return nullptr;

  return it->second;
}

bool StatisticsProviderImpl::GetRegionalInformation(const std::string& name,
                                                    std::string* result) const {
  if (region_.empty() || region_dict_.is_none())
    return false;

  const RegionDataExtractor extractor = GetRegionalDataExtractor(name);
  if (!extractor)
    return false;

  return extractor(region_dict_, result);
}

bool StatisticsProviderImpl::GetMachineStatistic(const std::string& name,
                                                 std::string* result) {
  VLOG(1) << "Machine Statistic requested: " << name;
  if (!WaitForStatisticsLoaded()) {
    LOG(ERROR) << "GetMachineStatistic called before load started: " << name;
    return false;
  }

  // Test region should override VPD values.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kCrosRegion) &&
      GetRegionalInformation(name, result)) {
    return true;
  }

  NameValuePairsParser::NameValueMap::iterator iter = machine_info_.find(name);
  if (iter == machine_info_.end()) {
    if (GetRegionalInformation(name, result))
      return true;
    if (result != nullptr && base::SysInfo::IsRunningOnChromeOS() &&
        (oem_manifest_loaded_ || !HasOemPrefix(name))) {
      VLOG(1) << "Requested statistic not found: " << name;
    }
    return false;
  }
  if (result != nullptr)
    *result = iter->second;
  return true;
}

bool StatisticsProviderImpl::GetMachineFlag(const std::string& name,
                                            bool* result) {
  VLOG(1) << "Machine Flag requested: " << name;
  if (!WaitForStatisticsLoaded()) {
    LOG(ERROR) << "GetMachineFlag called before load started: " << name;
    return false;
  }

  MachineFlags::const_iterator iter = machine_flags_.find(name);
  if (iter == machine_flags_.end()) {
    if (result != nullptr && base::SysInfo::IsRunningOnChromeOS() &&
        (oem_manifest_loaded_ || !HasOemPrefix(name))) {
      VLOG(1) << "Requested machine flag not found: " << name;
    }
    return false;
  }
  if (result != nullptr)
    *result = iter->second;
  return true;
}

void StatisticsProviderImpl::Shutdown() {
  cancellation_flag_.Set();  // Cancel any pending loads
}

bool StatisticsProviderImpl::IsRunningOnVm() {
  if (!base::SysInfo::IsRunningOnChromeOS())
    return false;
  std::string is_vm;
  return GetMachineStatistic(kIsVmKey, &is_vm) && is_vm == kIsVmValueTrue;
}

StatisticsProviderImpl::StatisticsProviderImpl()
    : load_statistics_started_(false),
      oem_manifest_loaded_(false),
      statistics_loaded_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {
  regional_data_extractors_[kInitialLocaleKey] =
      &GetInitialLocaleFromRegionalData;
  regional_data_extractors_[kKeyboardLayoutKey] =
      &GetKeyboardLayoutFromRegionalData;
  regional_data_extractors_[kKeyboardMechanicalLayoutKey] =
      &GetKeyboardMechanicalLayoutFromRegionalData;
  regional_data_extractors_[kInitialTimezoneKey] =
      &GetInitialTimezoneFromRegionalData;
}

StatisticsProviderImpl::~StatisticsProviderImpl() = default;

void StatisticsProviderImpl::StartLoadingMachineStatistics(
    bool load_oem_manifest) {
  CHECK(!load_statistics_started_);
  load_statistics_started_ = true;

  VLOG(1) << "Started loading statistics. Load OEM Manifest: "
          << load_oem_manifest;

  // TaskPriority::USER_BLOCKING because this is on the critical path of
  // rendering the NTP on startup. https://crbug.com/831835
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&StatisticsProviderImpl::LoadMachineStatistics,
                     base::Unretained(this), load_oem_manifest));
}

void StatisticsProviderImpl::ScheduleOnMachineStatisticsLoaded(
    base::OnceClosure callback) {
  {
    // It is important to hold |statistics_loaded_lock_| when checking the
    // |statistics_loaded_| event to make sure that its state doesn't change
    // before |callback| is added to |statistics_loaded_callbacks_|.
    base::AutoLock auto_lock(statistics_loaded_lock_);

    // Machine statistics are not loaded yet. Add |callback| to a list to be
    // scheduled once machine statistics are loaded.
    if (!statistics_loaded_.IsSignaled()) {
      statistics_loaded_callbacks_.emplace_back(
          std::move(callback), base::SequencedTaskRunnerHandle::Get());
      return;
    }
  }

  // Machine statistics are loaded. Schedule |callback| immediately.
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   std::move(callback));
}

void StatisticsProviderImpl::LoadMachineStatistics(bool load_oem_manifest) {
  // Run from the file task runner. StatisticsProviderImpl is a Singleton<> and
  // will not be destroyed until after threads have been stopped, so this test
  // is always safe.
  if (cancellation_flag_.IsSet())
    return;

  std::string crossystem_wpsw;
  NameValuePairsParser parser(&machine_info_);
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // Parse all of the key/value pairs from the crossystem tool.
    if (!parser.ParseNameValuePairsFromTool(
            std::size(kCrosSystemTool), kCrosSystemTool,
            NameValuePairsFormat::kCrossystem)) {
      LOG(ERROR) << "Errors parsing output from: " << kCrosSystemTool;
    }
    // Drop useless "(error)" values so they don't displace valid values
    // supplied later by other tools: https://crbug.com/844258
    parser.DeletePairsWithValue(kCrosSystemValueError);

    auto it = machine_info_.find(kFirmwareWriteProtectCurrentKey);
    if (it != machine_info_.end()) {
      crossystem_wpsw = it->second;
      machine_info_.erase(it);
    }
  }

  base::FilePath machine_info_path;
  base::PathService::Get(FILE_MACHINE_INFO, &machine_info_path);
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !base::PathExists(machine_info_path)) {
    // Use time value to create an unique stub serial because clashes of the
    // same serial for the same domain invalidate earlier enrollments. Persist
    // to disk to keep it constant across restarts (required for re-enrollment
    // testing).
    std::string stub_contents =
        "\"serial_number\"=\"stub_" +
        base::NumberToString(base::Time::Now().ToJavaTime()) + "\"\n";
    int bytes_written = base::WriteFile(
        machine_info_path, stub_contents.c_str(), stub_contents.size());
    if (bytes_written < static_cast<int>(stub_contents.size())) {
      PLOG(ERROR) << "Error writing machine info stub "
                  << machine_info_path.value();
    }
  }

  base::FilePath vpd_path;
  base::PathService::Get(FILE_VPD, &vpd_path);
  if (!base::PathExists(vpd_path)) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      ReportVpdCacheReadResult(VpdCacheReadResult::KMissing);
      LOG(ERROR) << "Missing FILE_VPD: " << vpd_path;
    } else {
      std::string stub_contents = "\"ActivateDate\"=\"2000-01\"\n";
      int bytes_written = base::WriteFile(vpd_path, stub_contents.c_str(),
                                          stub_contents.size());
      if (bytes_written < static_cast<int>(stub_contents.size())) {
        PLOG(ERROR) << "Error writing VPD stub " << vpd_path.value();
      }
    }
  }

  // The machine-info file is generated only for OOBE and enterprise enrollment
  // and may not be present. See login-manager/init/machine-info.conf.
  parser.ParseNameValuePairsFromFile(machine_info_path,
                                     NameValuePairsFormat::kMachineInfo);
  parser.ParseNameValuePairsFromFile(base::FilePath(kEchoCouponFile),
                                     NameValuePairsFormat::kVpdDump);
  bool vpd_parse_result = parser.ParseNameValuePairsFromFile(
      vpd_path, NameValuePairsFormat::kVpdDump);
  if (base::SysInfo::IsRunningOnChromeOS()) {
    if (vpd_parse_result) {
      ReportVpdCacheReadResult(VpdCacheReadResult::kSuccess);
    } else {
      ReportVpdCacheReadResult(VpdCacheReadResult::kParseFailed);
      LOG(ERROR) << "Failed to parse FILE_VPD: " << vpd_path;
    }
  }

  // Ensure that the hardware class key is present with the expected
  // key name, and if it couldn't be retrieved, that the value is "unknown".
  std::string hardware_class = machine_info_[kHardwareClassCrosSystemKey];
  machine_info_[kHardwareClassKey] =
      !hardware_class.empty() ? hardware_class : kHardwareClassValueUnknown;

  if (base::SysInfo::IsRunningOnChromeOS()) {
    // By default, assume that this is *not* a VM. If crossystem is not present,
    // report that we are not in a VM.
    machine_info_[kIsVmKey] = kIsVmValueFalse;
    const auto is_vm_iter = machine_info_.find(kIsVmCrosSystemKey);
    if (is_vm_iter != machine_info_.end() &&
        is_vm_iter->second == kIsVmValueTrue) {
      machine_info_[kIsVmKey] = kIsVmValueTrue;
    }

    // Use the write-protect value from crossystem only if it hasn't been loaded
    // from any other source, since the result of crosystem is less reliable for
    // this key.
    if (machine_info_.find(kFirmwareWriteProtectCurrentKey) ==
            machine_info_.end() &&
        !crossystem_wpsw.empty()) {
      machine_info_[kFirmwareWriteProtectCurrentKey] = crossystem_wpsw;
    }
  }

  if (load_oem_manifest) {
    // If kAppOemManifestFile switch is specified, load OEM Manifest file.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kAppOemManifestFile)) {
      LoadOemManifestFromFile(
          command_line->GetSwitchValuePath(switches::kAppOemManifestFile));
    } else if (base::SysInfo::IsRunningOnChromeOS()) {
      LoadOemManifestFromFile(base::FilePath(kOemManifestFilePath));
    }
  }

  // Set region
  const auto region_iter = machine_info_.find(kRegionKey);
  if (region_iter != machine_info_.end())
    region_ = region_iter->second;
  else
    region_ = std::string();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kCrosRegion)) {
    region_ = command_line->GetSwitchValueASCII(ash::switches::kCrosRegion);
    machine_info_[kRegionKey] = region_;
    VLOG(1) << "CrOS region set to '" << region_ << "'";
  }

  LoadRegionsFile(base::FilePath(kCrosRegions));

  SignalStatisticsLoaded();
}

void StatisticsProviderImpl::LoadRegionsFile(const base::FilePath& filename) {
  JSONFileValueDeserializer regions_file(filename);
  int regions_error_code = 0;
  std::string regions_error_message;
  std::unique_ptr<base::Value> json_value =
      regions_file.Deserialize(&regions_error_code, &regions_error_message);
  if (!json_value.get()) {
    if (base::SysInfo::IsRunningOnChromeOS())
      LOG(ERROR) << "Failed to load regions file '" << filename.value()
                 << "': error='" << regions_error_message << "'";

    return;
  }
  if (!json_value->is_dict()) {
    LOG(ERROR) << "Bad regions file '" << filename.value()
               << "': not a dictionary.";
    return;
  }

  base::Value* region_dict = json_value->FindDictKey(region_);
  if (region_dict == nullptr) {
    LOG(ERROR) << "Bad regional data: '" << region_ << "' << not found.";
    return;
  }
  region_dict_ = std::move(*region_dict);
}

void StatisticsProviderImpl::LoadOemManifestFromFile(
    const base::FilePath& file) {
  // Called from LoadMachineStatistics. Check cancellation_flag_ again here.
  if (cancellation_flag_.IsSet())
    return;

  KioskOemManifestParser::Manifest oem_manifest;
  if (!KioskOemManifestParser::Load(file, &oem_manifest)) {
    LOG(WARNING) << "Unable to load OEM Manifest file: " << file.value();
    return;
  }
  machine_info_[kOemDeviceRequisitionKey] = oem_manifest.device_requisition;
  machine_flags_[kOemIsEnterpriseManagedKey] = oem_manifest.enterprise_managed;
  machine_flags_[kOemCanExitEnterpriseEnrollmentKey] =
      oem_manifest.can_exit_enrollment;
  machine_flags_[kOemKeyboardDrivenOobeKey] = oem_manifest.keyboard_driven_oobe;

  oem_manifest_loaded_ = true;
  VLOG(1) << "Loaded OEM Manifest statistics from " << file.value();
}

StatisticsProviderImpl* StatisticsProviderImpl::GetInstance() {
  return base::Singleton<
      StatisticsProviderImpl,
      base::DefaultSingletonTraits<StatisticsProviderImpl>>::get();
}

std::string StatisticsProvider::GetEnterpriseMachineID() {
  std::string machine_id;
  for (const char* key : kMachineInfoSerialNumberKeys) {
    if (GetMachineStatistic(key, &machine_id) && !machine_id.empty()) {
      break;
    }
  }
  return machine_id;
}

static StatisticsProvider* g_test_statistics_provider = NULL;

// static
StatisticsProvider* StatisticsProvider::GetInstance() {
  if (g_test_statistics_provider)
    return g_test_statistics_provider;
  return StatisticsProviderImpl::GetInstance();
}

// static
void StatisticsProvider::SetTestProvider(StatisticsProvider* test_provider) {
  g_test_statistics_provider = test_provider;
}

}  // namespace system
}  // namespace chromeos
