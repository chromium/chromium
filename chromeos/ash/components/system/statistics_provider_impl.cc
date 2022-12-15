// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/statistics_provider_impl.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

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
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/system/kiosk_oem_manifest_parser.h"

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

const char kVpdRoPartitionStatusKey[] = "RO_VPD_status";
const char kVpdRwPartitionStatusKey[] = "RW_VPD_status";
const char kVpdPartitionStatusValid[] = "0";

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

// Gets ListValue from given `dictionary` by given `key`, and returns it as a
// string with all list values joined by ','. Returns nullopt if `key` is not
// found.
absl::optional<std::string> JoinListValuesToString(
    const base::Value& dictionary,
    base::StringPiece key) {
  const base::Value* list_value = dictionary.FindListKey(key);
  if (list_value == nullptr)
    return absl::nullopt;

  std::string buffer;
  bool first = true;
  for (const auto& v : list_value->GetList()) {
    const std::string* value = v.GetIfString();
    if (!value)
      return absl::nullopt;

    if (first)
      first = false;
    else
      buffer += ',';

    buffer += *value;
  }

  return buffer;
}

// Gets ListValue from given `dictionary` by given `key`, and returns the first
// value of the list as string. Returns nullopt if `key` is not found.
absl::optional<std::string> GetFirstListValueAsString(
    const base::Value& dictionary,
    base::StringPiece key) {
  const base::Value* list_value = dictionary.FindListKey(key);
  if (list_value == nullptr || list_value->GetList().empty())
    return absl::nullopt;

  const std::string* value = list_value->GetList()[0].GetIfString();
  if (value == nullptr)
    return absl::nullopt;

  return *value;
}

absl::optional<std::string> GetKeyboardLayoutFromRegionalData(
    const base::Value& region_dict) {
  return JoinListValuesToString(region_dict, kKeyboardsPath);
}

absl::optional<std::string> GetKeyboardMechanicalLayoutFromRegionalData(
    const base::Value& region_dict) {
  const std::string* value =
      region_dict.FindStringPath(kKeyboardMechanicalLayoutPath);
  if (value == nullptr)
    return absl::nullopt;

  return *value;
}

absl::optional<std::string> GetInitialTimezoneFromRegionalData(
    const base::Value& region_dict) {
  return GetFirstListValueAsString(region_dict, kTimeZonesPath);
}

absl::optional<std::string> GetInitialLocaleFromRegionalData(
    const base::Value& region_dict) {
  return JoinListValuesToString(region_dict, kLocalesPath);
}

// Array mapping region keys to their extracting functions.
constexpr std::pair<const char*,
                    absl::optional<std::string> (*)(const base::Value&)>
    kRegionKeysToExtractors[] = {
        {kInitialLocaleKey, &GetInitialLocaleFromRegionalData},
        {kKeyboardLayoutKey, &GetKeyboardLayoutFromRegionalData},
        {kKeyboardMechanicalLayoutKey,
         &GetKeyboardMechanicalLayoutFromRegionalData},
        {kInitialTimezoneKey, &GetInitialTimezoneFromRegionalData}};

void ReportVpdCacheReadResult(
    StatisticsProviderImpl::VpdCacheReadResult result) {
  base::UmaHistogramEnumeration("Enterprise.VPDCacheReadResult", result);
}

base::FilePath GetFilePathIgnoreFailure(int key) {
  base::FilePath file_path;
  base::PathService::Get(key, &file_path);

  return file_path;
}

bool HasOemPrefix(base::StringPiece name) {
  return name.substr(0, 4) == "oem_";
}

StatisticsProviderImpl::StatisticsSources CreateDefaultSources() {
  StatisticsProviderImpl::StatisticsSources sources;
  sources.crossystem_tool = base::CommandLine(base::FilePath(kCrosSystemTool));
  sources.machine_info_filepath = GetFilePathIgnoreFailure(FILE_MACHINE_INFO);
  sources.vpd_echo_filepath = base::FilePath(kEchoCouponFile);
  sources.vpd_filepath = GetFilePathIgnoreFailure(FILE_VPD);
  sources.vpd_status_filepath = GetFilePathIgnoreFailure(FILE_VPD_STATUS);
  sources.oem_manifest_filepath = base::FilePath(kOemManifestFilePath);
  sources.cros_regions_filepath = base::FilePath(kCrosRegions);
  return sources;
}

// Reads `vpd_status_file`, and loads and checks VPD key-value statuses from it.
// Returns VpdStatus according to file existence and content.
StatisticsProvider::VpdStatus LoadVpdStatusFile(
    const base::FilePath& vpd_status_file) {
  using Status = StatisticsProvider::VpdStatus;
  if (!base::PathExists(vpd_status_file)) {
    return Status::kInvalid;
  }

  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);

  if (!parser.ParseNameValuePairsFromFile(vpd_status_file,
                                          NameValuePairsFormat::kVpdDump)) {
    // Failed to parse one of the values in the status file. Let's still check
    // if partitions statuses are present. It is safe to ignore malformed
    // values because a missing key is considered as invalid state.
    LOG(ERROR) << "Failed to parse VPD status file: " << vpd_status_file;
  }

  const auto ro_vpd_it = map.find(kVpdRoPartitionStatusKey);
  const bool is_ro_vpd_valid =
      ro_vpd_it != map.end() && ro_vpd_it->second == kVpdPartitionStatusValid;
  LOG_IF(ERROR, !is_ro_vpd_valid)
      << "RO_VPD partition has non-valid status: '"
      << (ro_vpd_it == map.end() ? "value missing" : ro_vpd_it->second) << "'";

  const auto rw_vpd_it = map.find(kVpdRwPartitionStatusKey);
  const bool is_rw_vpd_valid =
      rw_vpd_it != map.end() && rw_vpd_it->second == kVpdPartitionStatusValid;
  LOG_IF(ERROR, !is_rw_vpd_valid)
      << "RW_VPD partition has non-valid status: '"
      << (rw_vpd_it == map.end() ? "value missing" : rw_vpd_it->second) << "'";

  return is_ro_vpd_valid
             ? (is_rw_vpd_valid ? Status::kValid : Status::kRwInvalid)
             : (is_rw_vpd_valid ? Status::kRoInvalid : Status::kInvalid);
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
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {}

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
          std::move(callback), base::SequencedTaskRunner::GetCurrentDefault());
      return;
    }
  }

  // Machine statistics are loaded. Schedule `callback` immediately.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

absl::optional<base::StringPiece> StatisticsProviderImpl::GetMachineStatistic(
    base::StringPiece name) {
  VLOG(1) << "Machine Statistic requested: " << name;
  if (!WaitForStatisticsLoaded()) {
    LOG(ERROR) << "GetMachineStatistic called before load started: " << name;
    return absl::nullopt;
  }

  // Test region should override any other value.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kCrosRegion)) {
    if (const absl::optional<base::StringPiece> region_result =
            GetRegionalInformation(name))
      return region_result;
  }

  if (const auto iter = machine_info_.find(name); iter != machine_info_.end()) {
    return base::StringPiece(iter->second);
  }

  if (const absl::optional<base::StringPiece> region_result =
          GetRegionalInformation(name)) {
    return region_result;
  }

  if (base::SysInfo::IsRunningOnChromeOS() &&
      (oem_manifest_loaded_ || !HasOemPrefix(name))) {
    VLOG(1) << "Requested statistic not found: " << name;
  }

  return absl::nullopt;
}

StatisticsProviderImpl::FlagValue StatisticsProviderImpl::GetMachineFlag(
    base::StringPiece name) {
  VLOG(1) << "Machine Flag requested: " << name;
  if (!WaitForStatisticsLoaded()) {
    LOG(ERROR) << "GetMachineFlag called before load started: " << name;
    return FlagValue::kUnset;
  }

  if (const auto iter = machine_flags_.find(name);
      iter != machine_flags_.end()) {
    return iter->second ? FlagValue::kTrue : FlagValue::kFalse;
  }

  if (base::SysInfo::IsRunningOnChromeOS() &&
      (oem_manifest_loaded_ || !HasOemPrefix(name))) {
    VLOG(1) << "Requested machine flag not found: " << name;
  }

  return FlagValue::kUnset;
}

void StatisticsProviderImpl::Shutdown() {
  cancellation_flag_.Set();  // Cancel any pending loads
}

bool StatisticsProviderImpl::IsRunningOnVm() {
  if (!base::SysInfo::IsRunningOnChromeOS())
    return false;
  return GetMachineStatistic(kIsVmKey) == kIsVmValueTrue;
}

StatisticsProvider::VpdStatus StatisticsProviderImpl::GetVpdStatus() const {
  return vpd_status_;
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

  LoadCrossystemTool();

  std::string crossystem_wpsw;

  if (base::SysInfo::IsRunningOnChromeOS()) {
    // If available, the key should be taken from machine info or VPD instead of
    // the tool. If not available, the tool's value will be restored.
    auto it = machine_info_.find(kFirmwareWriteProtectCurrentKey);
    if (it != machine_info_.end()) {
      crossystem_wpsw = it->second;
      machine_info_.erase(it);
    }
  }

  LoadMachineInfoFile();
  LoadVpdFiles();

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
      LOG(WARNING) << "wpsw_cur missing from machine_info, using value: "
                   << crossystem_wpsw;
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

  // Set region from command line if present.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kCrosRegion)) {
    const std::string region =
        command_line->GetSwitchValueASCII(ash::switches::kCrosRegion);
    machine_info_[kRegionKey] = region;
    VLOG(1) << "CrOS region set to '" << region << "'";
  }

  LoadRegionsFile(sources_.cros_regions_filepath,
                  machine_info_.find(kRegionKey) != machine_info_.end()
                      ? machine_info_[kRegionKey]
                      : "");

  SignalStatisticsLoaded();
}

void StatisticsProviderImpl::LoadCrossystemTool() {
  if (!base::SysInfo::IsRunningOnChromeOS()) {
    return;
  }

  NameValuePairsParser parser(&machine_info_);
  // Parse all of the key/value pairs from the crossystem tool.
  if (!parser.ParseNameValuePairsFromTool(sources_.crossystem_tool,
                                          NameValuePairsFormat::kCrossystem)) {
    LOG(ERROR) << "Errors parsing output from: "
               << sources_.crossystem_tool.GetProgram();
  }

  // Drop useless "(error)" values so they don't displace valid values
  // supplied later by other tools: https://crbug.com/844258
  parser.DeletePairsWithValue(kCrosSystemValueError);
}

void StatisticsProviderImpl::LoadMachineInfoFile() {
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !base::PathExists(sources_.machine_info_filepath)) {
    // Use time value to create an unique stub serial because clashes of the
    // same serial for the same domain invalidate earlier enrollments. Persist
    // to disk to keep it constant across restarts (required for re-enrollment
    // testing).
    std::string stub_contents =
        "\"serial_number\"=\"stub_" +
        base::NumberToString(base::Time::Now().ToJavaTime()) + "\"\n";
    int bytes_written =
        base::WriteFile(sources_.machine_info_filepath, stub_contents.c_str(),
                        stub_contents.size());
    if (bytes_written < static_cast<int>(stub_contents.size())) {
      PLOG(ERROR) << "Error writing machine info stub "
                  << sources_.machine_info_filepath;
    }
  }

  // The machine-info file is generated only for OOBE and enterprise enrollment
  // and may not be present. See login-manager/init/machine-info.conf.
  NameValuePairsParser(&machine_info_)
      .ParseNameValuePairsFromFile(sources_.machine_info_filepath,
                                   NameValuePairsFormat::kMachineInfo);
}

void StatisticsProviderImpl::LoadVpdFiles() {
  NameValuePairsParser parser(&machine_info_);

  parser.ParseNameValuePairsFromFile(sources_.vpd_echo_filepath,
                                     NameValuePairsFormat::kVpdDump);

  if (!base::PathExists(sources_.vpd_filepath)) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      // The actual VPD file is missing and there's nothing to load. Record the
      // metric and continue with loading the next source.
      ReportVpdCacheReadResult(VpdCacheReadResult::KMissing);
      LOG(ERROR) << "Missing FILE_VPD: " << sources_.vpd_filepath;
      vpd_status_ = VpdStatus::kInvalid;
      return;
    } else {
      std::string stub_contents = "\"ActivateDate\"=\"2000-01\"\n";
      int bytes_written = base::WriteFile(
          sources_.vpd_filepath, stub_contents.c_str(), stub_contents.size());
      if (bytes_written < static_cast<int>(stub_contents.size())) {
        PLOG(ERROR) << "Error writing VPD stub " << sources_.vpd_filepath;
      }
    }
  }

  const bool vpd_parse_result = parser.ParseNameValuePairsFromFile(
      sources_.vpd_filepath, NameValuePairsFormat::kVpdDump);
  if (base::SysInfo::IsRunningOnChromeOS()) {
    if (vpd_parse_result) {
      ReportVpdCacheReadResult(VpdCacheReadResult::kSuccess);
    } else {
      ReportVpdCacheReadResult(VpdCacheReadResult::kParseFailed);
      LOG(ERROR) << "Failed to parse FILE_VPD: " << sources_.vpd_filepath;
    }
  }

  vpd_status_ = LoadVpdStatusFile(sources_.vpd_status_filepath);

  LOG_IF(ERROR, vpd_status_ != VpdStatus::kValid)
      << "Detected invalid VPD state: "
      << static_cast<std::underlying_type_t<VpdStatus>>(vpd_status_);
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

void StatisticsProviderImpl::LoadRegionsFile(const base::FilePath& filename,
                                             base::StringPiece region) {
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

  base::Value* region_dict = json_value->FindDictKey(region);
  if (region_dict == nullptr) {
    LOG(ERROR) << "Bad regional data: '" << region << "' << not found.";
    return;
  }

  // Extract region keys from the dictionary with corresponding extractors.
  for (const auto& [key, extractor] : kRegionKeysToExtractors) {
    if (auto region_statistic = extractor(*region_dict)) {
      region_info_[key] = std::move(region_statistic.value());
    }
  }
}

absl::optional<base::StringPiece>
StatisticsProviderImpl::GetRegionalInformation(base::StringPiece name) const {
  if (machine_info_.find(kRegionKey) == machine_info_.end())
    return absl::nullopt;

  if (const auto iter = region_info_.find(name); iter != region_info_.end())
    return base::StringPiece(iter->second);

  return absl::nullopt;
}

}  // namespace chromeos::system
