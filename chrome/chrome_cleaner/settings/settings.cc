// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/settings/settings.h"

#include <algorithm>
#include <set>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/win/win_util.h"
#include "chrome/chrome_cleaner/buildflags.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/settings/engine_settings.h"
#include "chrome/chrome_cleaner/settings/settings_definitions.h"
#include "mojo/public/cpp/platform/platform_channel.h"

namespace chrome_cleaner {

namespace {

// Returns the value associated with flag --session-id when it's present or
// empty string when it's not found.
base::string16 GetSessionId(const base::CommandLine& command_line) {
  return command_line.GetSwitchValueNative(kSessionIdSwitch);
}

// Returns the engine being used.
Engine::Name GetEngine(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(kEngineSwitch)) {
    std::string value = command_line.GetSwitchValueASCII(kEngineSwitch);
    int numeric_value = Engine::UNKNOWN;
    if (base::StringToInt(value, &numeric_value) &&
        Engine_Name_IsValid(numeric_value) &&
        numeric_value != Engine::UNKNOWN &&
        numeric_value != Engine::DEPRECATED_URZA) {
      return static_cast<Engine::Name>(numeric_value);
    }

    LOG(WARNING) << "Invalid engine (" << value << "), using default engine "
                 << GetDefaultEngine();
  }

  return GetDefaultEngine();
}

ExecutionMode GetExecutionMode(const base::CommandLine& command_line) {
  int val = -1;
  // WARNING: this switch is used by internal test systems, be careful when
  // making changes.
  if (base::StringToInt(command_line.GetSwitchValueASCII(kExecutionModeSwitch),
                        &val) &&
      val > static_cast<int>(ExecutionMode::kNone) &&
      val < static_cast<int>(ExecutionMode::kNumValues)) {
    return static_cast<ExecutionMode>(val);
  }
  return ExecutionMode::kNone;
}

bool GetLogsCollectionEnabled(const base::CommandLine& command_line,
                              TargetBinary target_binary,
                              ExecutionMode execution_mode) {
  switch (target_binary) {
    case TargetBinary::kReporter:
      // For the reporter, if logging collection is enabled, then we will save
      // UwS matching to a proto that can be saved to disk or sent to Google if
      // logs upload is allowed. WARNING: this switch is used by internal test
      // systems, be careful when making changes.
      return command_line.HasSwitch(kExtendedSafeBrowsingEnabledSwitch);
    case TargetBinary::kCleaner:
      // Logs collection is only enabled if the user did not opt out, which
      // causes the scanner process to launch the elevated cleaner with the
      // |kWithCleanupModeLogsSwitch| flag.
      return (execution_mode == ExecutionMode::kCleanup &&
              command_line.HasSwitch(kWithCleanupModeLogsSwitch)) ||
             (execution_mode == ExecutionMode::kScanning &&
              command_line.HasSwitch(kWithScanningModeLogsSwitch));
    default:
      return false;
  }
}

bool GetLogsUploadAllowed(const base::CommandLine& command_line,
                          TargetBinary target_binary,
                          ExecutionMode execution_mode) {
  // WARNING: this switch is used by internal test systems, be careful when
  // making changes.
  if (command_line.HasSwitch(kNoReportUploadSwitch))
    return false;

#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  // Unofficial builds upload logs only if test a logging URL is specified.
  if (!command_line.HasSwitch(kTestLoggingURLSwitch))
    return false;
#endif

  return GetLogsCollectionEnabled(command_line, target_binary, execution_mode);
}

bool GetAllowCrashReportUpload(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(kNoCrashUploadSwitch))
    return false;

  // WARNING: this switch is used by internal test systems, be careful when
  // making changes.
  return command_line.HasSwitch(kEnableCrashReportingSwitch);
}

// If switch --cleanup-id is present and not empty, returns the value
// associated with it. The most common case is when the current cleaner
// process is running post-reboot and was scheduled by a pre-reboot process
// with that cleanup id. Otherwise, generate a new random string that should be
// immutable for this process and propagated to other processes corresponding
// to the same cleanup.
std::string GetCleanerRunId(const base::CommandLine& command_line) {
  std::string cleanup_id = command_line.GetSwitchValueASCII(kCleanupIdSwitch);
  if (cleanup_id.empty()) {
    cleanup_id = base::GenerateGUID();
    DCHECK(!cleanup_id.empty());
  }
  return cleanup_id;
}

// Populates |result| with locations that should be scanned. Returns true if no
// invalid location values were provided on the command line.
bool GetLocationsToScan(const base::CommandLine& command_line,
                        TargetBinary target_binary,
                        std::vector<UwS::TraceLocation>* result) {
  std::vector<UwS::TraceLocation> valid_locations = GetValidTraceLocations();
  if (!command_line.HasSwitch(kScanLocationsSwitch)) {
    // Do not scan Program Files or CLSID in the reporter since they are slow.
    if (target_binary == TargetBinary::kReporter) {
      base::Erase(valid_locations, UwS::FOUND_IN_CLSID);
      base::Erase(valid_locations, UwS::FOUND_IN_PROGRAMFILES);
    }
    result->swap(valid_locations);
    return true;
  }

  std::string locations_list_str =
      command_line.GetSwitchValueASCII(kScanLocationsSwitch);
  std::vector<base::StringPiece> location_strs =
      base::SplitStringPiece(locations_list_str, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  const std::set<UwS::TraceLocation> valid_locations_set(
      valid_locations.begin(), valid_locations.end());
  bool all_values_valid = true;
  result->clear();
  for (const auto& location_str : location_strs) {
    int location_num;
    if (base::StringToInt(location_str, &location_num)) {
      UwS::TraceLocation location =
          static_cast<UwS::TraceLocation>(location_num);
      if (valid_locations_set.find(location) != valid_locations_set.end())
        result->push_back(location);
      else
        all_values_valid = false;
    } else {
      all_values_valid = false;
    }
  }

  if (result->empty()) {
    result->swap(valid_locations);
    all_values_valid = false;
  }

  return all_values_valid;
}

int64_t GetOpenFileSizeLimit(const base::CommandLine& command_line,
                             TargetBinary target_binary) {
  int64_t result;
  if (target_binary == TargetBinary::kReporter) {
    std::string open_file_size_limit_str =
        command_line.GetSwitchValueASCII(kFileSizeLimitSwitch);
    if (base::StringToInt64(open_file_size_limit_str, &result) && result > 0)
      return result;
  }
  return 0;
}

}  // namespace

bool GetTimeoutOverride(const base::CommandLine& command_line,
                        const char* switch_name,
                        base::TimeDelta* timeout) {
  if (!command_line.HasSwitch(switch_name))
    return false;

  int timeout_minutes = 0;
  if (!base::StringToInt(command_line.GetSwitchValueNative(switch_name),
                         &timeout_minutes)) {
    return false;
  }

  // Negative timeout is invalid.
  if (timeout_minutes < 0)
    return false;

  DCHECK(timeout);
  *timeout = base::TimeDelta::FromMinutes(timeout_minutes);
  return true;
}

std::vector<UwS::TraceLocation> GetValidTraceLocations() {
  std::vector<UwS::TraceLocation> result;
  for (int location = UwS::TraceLocation_MIN;
       location <= UwS::TraceLocation_MAX; ++location) {
    if (UwS::TraceLocation_IsValid(location)) {
      result.push_back(static_cast<UwS::TraceLocation>(location));
    }
  }
  return result;
}

// static
Settings* Settings::instance_for_testing_ = nullptr;

// static
Settings* Settings::GetInstance() {
  if (instance_for_testing_)
    return instance_for_testing_;
  return base::Singleton<Settings>::get();
}

// static
void Settings::SetInstanceForTesting(Settings* instance_for_testing) {
  instance_for_testing_ = instance_for_testing;
}

bool Settings::allow_crash_report_upload() const {
  return allow_crash_report_upload_;
}

base::string16 Settings::session_id() const {
  return session_id_;
}

std::string Settings::cleanup_id() const {
  return cleanup_id_;
}

Engine::Name Settings::engine() const {
  return engine_;
}

std::string Settings::engine_version() const {
  return GetEngineVersion(engine());
}

bool Settings::logs_upload_allowed() const {
  return logs_upload_allowed_;
}

bool Settings::logs_collection_enabled() const {
  return logs_collection_enabled_;
}

bool Settings::logs_allowed_in_cleanup_mode() const {
  DCHECK_EQ(ExecutionMode::kScanning, execution_mode_);
  return logs_allowed_in_cleanup_mode_;
}

void Settings::set_logs_allowed_in_cleanup_mode(bool new_value) {
  // TODO(joenotcharles): Make the global settings object immutable.
  DCHECK_EQ(ExecutionMode::kScanning, execution_mode_);
  logs_allowed_in_cleanup_mode_ = new_value;
}

bool Settings::metrics_enabled() const {
  return metrics_enabled_;
}

bool Settings::sber_enabled() const {
  return sber_enabled_;
}

const std::string& Settings::chrome_mojo_pipe_token() const {
  return chrome_mojo_pipe_token_;
}

bool Settings::prompt_using_mojo() const {
  return prompt_using_mojo_;
}

HANDLE Settings::prompt_response_read_handle() const {
  return prompt_response_read_handle_;
}

HANDLE Settings::prompt_request_write_handle() const {
  return prompt_request_write_handle_;
}

bool Settings::switches_valid_for_ipc() const {
  // IPC is only used in scanning mode. In other modes ignore the flags.
  if (execution_mode() != ExecutionMode::kScanning) {
    return true;
  }

  // Only one IPC mechanism can be used.
  if (prompt_using_mojo_ && prompt_using_proto_) {
    return false;
  }

  // At least one IPC mechanism has to be used.
  if (!prompt_using_mojo_ && !prompt_using_proto_) {
    return false;
  }

  // Mojo use requires two flags.
  if (prompt_using_mojo_) {
    if (chrome_mojo_pipe_token().empty() || !has_parent_pipe_handle()) {
      return false;
    }
  }

  // Proto use requires two flags.
  if (prompt_using_proto_) {
    if (prompt_response_read_handle_ == INVALID_HANDLE_VALUE ||
        prompt_request_write_handle_ == INVALID_HANDLE_VALUE) {
      return false;
    }
  }

  return true;
}

bool Settings::has_any_ipc_switch() const {
  return !chrome_mojo_pipe_token_.empty() || has_parent_pipe_handle_ ||
         prompt_response_read_handle_ != INVALID_HANDLE_VALUE ||
         prompt_request_write_handle_ != INVALID_HANDLE_VALUE;
}

bool Settings::has_parent_pipe_handle() const {
  return has_parent_pipe_handle_;
}

ExecutionMode Settings::execution_mode() const {
  return execution_mode_;
}

bool Settings::remove_report_only_uws() const {
  return remove_report_only_uws_;
}

bool Settings::cleaning_timeout_overridden() const {
  return cleaning_timeout_overridden_;
}

base::TimeDelta Settings::cleaning_timeout() const {
  return cleaning_timeout_;
}

bool Settings::scanning_timeout_overridden() const {
  return scanning_timeout_overridden_;
}

base::TimeDelta Settings::scanning_timeout() const {
  return scanning_timeout_;
}

bool Settings::user_response_timeout_overridden() const {
  return user_response_timeout_overridden_;
}

base::TimeDelta Settings::user_response_timeout() const {
  return user_response_timeout_;
}

const std::vector<UwS::TraceLocation>& Settings::locations_to_scan() const {
  return locations_to_scan_;
}

bool Settings::scan_switches_correct() const {
  return scan_switches_correct_;
}

int64_t Settings::open_file_size_limit() const {
  return open_file_size_limit_;
}

bool Settings::run_without_sandbox_for_testing() const {
  return run_without_sandbox_for_testing_;
}

Settings::Settings() {
  Initialize(*base::CommandLine::ForCurrentProcess(), GetTargetBinary());
}

Settings::~Settings() = default;

void Settings::Initialize(const base::CommandLine& command_line,
                          TargetBinary target_binary) {
  allow_crash_report_upload_ = GetAllowCrashReportUpload(command_line);
  session_id_ = GetSessionId(command_line);
  cleanup_id_ = GetCleanerRunId(command_line);
  engine_ = GetEngine(command_line);
  DCHECK_NE(engine_, Engine::UNKNOWN);

  metrics_enabled_ = command_line.HasSwitch(kUmaUserSwitch);
  // WARNING: this switch is used by internal test systems, be careful when
  // making changes.
  sber_enabled_ = command_line.HasSwitch(kExtendedSafeBrowsingEnabledSwitch);
  execution_mode_ = GetExecutionMode(command_line);
  logs_upload_allowed_ =
      GetLogsUploadAllowed(command_line, target_binary, execution_mode_);
  logs_collection_enabled_ =
      GetLogsCollectionEnabled(command_line, target_binary, execution_mode_);

  // Mojo related.
  chrome_mojo_pipe_token_ = command_line.GetSwitchValueASCII(
      chrome_cleaner::kChromeMojoPipeTokenSwitch);
  has_parent_pipe_handle_ =
      command_line.HasSwitch(mojo::PlatformChannel::kHandleSwitch);
  if (!chrome_mojo_pipe_token_.empty() || has_parent_pipe_handle_) {
    prompt_using_mojo_ = true;
  }

  // Proto related.
  uint32_t handle_value;
  if (base::StringToUint(command_line.GetSwitchValueNative(
                             chrome_cleaner::kChromeReadHandleSwitch),
                         &handle_value)) {
    prompt_response_read_handle_ = base::win::Uint32ToHandle(handle_value);
  }
  if (base::StringToUint(command_line.GetSwitchValueNative(
                             chrome_cleaner::kChromeWriteHandleSwitch),
                         &handle_value)) {
    prompt_request_write_handle_ = base::win::Uint32ToHandle(handle_value);
  }

  if (prompt_response_read_handle_ != INVALID_HANDLE_VALUE ||
      prompt_request_write_handle_ != INVALID_HANDLE_VALUE) {
    prompt_using_proto_ = true;
  }

#if !BUILDFLAG(IS_OFFICIAL_CHROME_CLEANER_BUILD)
  remove_report_only_uws_ = command_line.HasSwitch(kRemoveScanOnlyUwS);
  run_without_sandbox_for_testing_ =
      command_line.HasSwitch(kRunWithoutSandboxForTestingSwitch);
#endif

  cleaning_timeout_overridden_ = GetTimeoutOverride(
      command_line, kCleaningTimeoutMinutesSwitch, &cleaning_timeout_);
  scanning_timeout_overridden_ = GetTimeoutOverride(
      command_line, kScanningTimeoutMinutesSwitch, &scanning_timeout_);
  user_response_timeout_overridden_ = GetTimeoutOverride(
      command_line, kUserResponseTimeoutMinutesSwitch, &user_response_timeout_);
  scan_switches_correct_ =
      GetLocationsToScan(command_line, target_binary, &locations_to_scan_);
  open_file_size_limit_ = GetOpenFileSizeLimit(command_line, target_binary);
}

}  // namespace chrome_cleaner
