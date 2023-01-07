// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/statistics_provider_impl.h"

#include <string>

#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/system/kiosk_oem_manifest_parser.h"

namespace chromeos::system {

namespace {

// Path to the tool used to get system info, and special values for the
// output of the tool.
const char kCrosSystemTool[] = "/usr/bin/crossystem";
const char kCrosSystemValueError[] = "(error)";

// File to get ECHO coupon info from, and key/value delimiters of
// the file.
const char kEchoCouponFile[] =
    "/mnt/stateful_partition/unencrypted/cache/vpd/echo/vpd_echo.txt";

// The location of OEM manifest file used to trigger OOBE flow for kiosk mode.
const base::CommandLine::CharType kOemManifestFilePath[] =
    FILE_PATH_LITERAL("/usr/share/oem/oobe/manifest.json");

// File to get regional data from.
const char kCrosRegions[] = "/usr/share/misc/cros-regions.json";

const char kHardwareClassCrosSystemKey[] = "hwid";
const char kHardwareClassValueUnknown[] = "unknown";

const char kIsVmCrosSystemKey[] = "inside_vm";

// Items in region dictionary.
const char kKeyboardsPath[] = "keyboards";
const char kLocalesPath[] = "locales";
const char kTimeZonesPath[] = "time_zones";
const char kKeyboardMechanicalLayoutPath[] = "keyboard_mechanical_layout";

// Timeout that we should wait for statistics to get loaded.
constexpr base::TimeDelta kLoadTimeout = base::Seconds(3);

// Gets ListValue from given `dictionary` by given `key` and (unless `result` is
// nullptr) sets `result` to a string with all list values joined by ','.
// Returns true on success.
bool JoinListValuesToString(const base::Value& dictionary,
                            const std::string key,
                            std::string* result) {
  const base::Value* list_value = dictionary.FindListKey(key);
  if (list_value == nullptr)
    return false;

  std::string buffer;
  bool first = true;
  for (const auto& v : list_value->GetList()) {
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

// Gets ListValue from given `dictionary` by given `key` and (unless `result` is
// nullptr) sets `result` to the first value as string.  Returns true on
// success.
bool GetFirstListValueAsString(const base::Value& dictionary,
                               const std::string key,
                               std::string* result) {
  const base::Value* list_value = dictionary.FindListKey(key);
  if (list_value == nullptr || list_value->GetList().empty())
    return false;

  const std::string* value = list_value->GetList()[0].GetIfString();
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

void ReportVpdCacheReadResult(
    StatisticsProviderImpl::VpdCacheReadResult result) {
  base::UmaHistogramEnumeration("Enterprise.VPDCacheReadResult", result);
}

base::FilePath GetFilePathIgnoreFailure(int key) {
  base::FilePath file_path;
  base::PathService::Get(key, &file_path);

  return file_path;
}

bool HasOemPrefix(const std::string& name) {
  return name.substr(0, 4) == "oem_";
}

StatisticsProviderImpl::StatisticsSources CreateDefaultSources() {
  StatisticsProviderImpl::StatisticsSources sources;
  sources.crossystem_tool = base::CommandLine(base::FilePath(kCrosSystemTool));
  sources.machine_info_filepath = GetFilePathIgnoreFailure(FILE_MACHINE_INFO);
  sources.vpd_echo_filepath = base::FilePath(kEchoCouponFile);
  sources.vpd_filepath = GetFilePathIgnoreFailure(FILE_VPD);
  sources.oem_manifest_filepath = base::FilePath(kOemManifestFilePath);
  sources.cros_regions_filepath = base::FilePath(kCrosRegions);
  return sources;
}

}  // namespace

const char kMetricVpdCacheReadResult[] = "Enterprise.VPDCacheReadResult";

StatisticsProviderImpl::StatisticsSources::StatisticsSources() = default;

StatisticsProviderImpl::StatisticsSources::~StatisticsSources() = default;

StatisticsProviderImpl::StatisticsSources::StatisticsSources(
    const StatisticsSources& other) = default;
StatisticsProviderImpl::StatisticsSources&
StatisticsProviderImpl::StatisticsSources::operator=(
    const StatisticsSources& other) = default;

StatisticsProviderImpl::StatisticsSources::StatisticsSources(
    StatisticsSources&& other) = default;
StatisticsProviderImpl::StatisticsSources&
StatisticsProviderImpl::StatisticsSources::operator=(
    StatisticsSources&& other) = default;

// static
std::unique_ptr<StatisticsProviderImpl>
StatisticsProviderImpl::CreateProviderForTesting(
    StatisticsSources testing_sources) {
  // Using `new` to access a non-public constructor.
  return base::WrapUnique(
      new StatisticsProviderImpl(std::move(testing_sources)));
}

StatisticsProviderImpl::StatisticsProviderImpl()
    : StatisticsProviderImpl(CreateDefaultSources()) {}

StatisticsProviderImpl::StatisticsProviderImpl(StatisticsSources sources)
    : sources_(std::move(sources)),
      load_statistics_started_(false),
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
    // It is important to hold `statistics_loaded_lock_` when checking the
    // `statistics_loaded_` event to make sure that its state doesn't change
    // before `callback` is added to `statistics_loaded_callbacks_`.
    base::AutoLock auto_lock(statistics_loaded_lock_);

    // Machine statistics are not loaded yet. Add `callback` to a list to be
    // scheduled once machine statistics are loaded.
    if (!statistics_loaded_.IsSignaled()) {
      statistics_loaded_callbacks_.emplace_back(
          std::move(callback), base::SequencedTaskRunnerHandle::Get());
      return;
    }
  }

  // Machine statistics are loaded. Schedule `callback` immediately.
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   std::move(callback));
}

bool StatisticsProviderImpl::GetMachineStatistic(const std::string& name,
                                                 std::string* result) {
  VLOG(1) << "Machine Statistic requested: " << name;
  if (!WaitForStatisticsLoaded()) {
    LOG(ERROR) << "GetMachineStatistic called before load started: " << name;
    return false;
  }

  // Test region should override any other value.
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

void StatisticsProviderImpl::SignalStatisticsLoaded() {
  decltype(statistics_loaded_callbacks_) local_statistics_loaded_callbacks;

  {
    base::AutoLock auto_lock(statistics_loaded_lock_);

    // Move all callbacks to a local variable.
    local_statistics_loaded_callbacks = std::move(statistics_loaded_callbacks_);

    // Prevent new callbacks from being added to `statistics_loaded_callbacks_`
    // and unblock pending WaitForStatisticsLoaded() calls.
    statistics_loaded_.Signal();

    VLOG(1) << "Finished loading statistics.";
  }

  // Schedule callbacks that were in `statistics_loaded_callbacks_`.
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
  statistics_loaded_.TimedWait(kLoadTimeout);

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
            sources_.crossystem_tool, NameValuePairsFormat::kCrossystem)) {
      LOG(ERROR) << "Errors parsing output from: "
                 << sources_.crossystem_tool.GetProgram();
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

  const base::FilePath& machine_info_path = sources_.machine_info_filepath;
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

  const base::FilePath& vpd_path = sources_.vpd_filepath;
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
  parser.ParseNameValuePairsFromFile(sources_.vpd_echo_filepath,
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
    // from any other source, since the result of crossystem is less reliable
    // for this key.
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
      LoadOemManifestFromFile(sources_.oem_manifest_filepath);
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

  LoadRegionsFile(sources_.cros_regions_filepath);

  SignalStatisticsLoaded();
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

bool StatisticsProviderImpl::GetRegionalInformation(const std::string& name,
                                                    std::string* result) const {
  if (region_.empty() || region_dict_.is_none())
    return false;

  const RegionDataExtractor extractor = GetRegionalDataExtractor(name);
  if (!extractor)
    return false;

  return extractor(region_dict_, result);
}

StatisticsProviderImpl::RegionDataExtractor
StatisticsProviderImpl::GetRegionalDataExtractor(
    const std::string& name) const {
  const auto it = regional_data_extractors_.find(name);
  if (it == regional_data_extractors_.end())
    return nullptr;

  return it->second;
}

}  // namespace chromeos::system
