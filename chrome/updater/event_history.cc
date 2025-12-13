// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/event_history.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/numerics/clamped_math.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <aclapi.h>
#include <sddl.h>

#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#endif

namespace updater {
namespace {

base::Lock& GetLoggingLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

base::File& GetLogFile() {
  static base::NoDestructor<base::File> file;
  return *file;
}

std::string UninstallPingReasonToString(UninstallPingReason reason) {
  switch (reason) {
    case UninstallPingReason::kUninstalled:
      return "UNINSTALLED";
    case UninstallPingReason::kUserNotAnOwner:
      return "USER_NOT_AN_OWNER";
    case UninstallPingReason::kNoAppsRemain:
      return "NO_APPS_REMAIN";
    case UninstallPingReason::kNeverHadApps:
      return "NEVER_HAD_APPS";
  }
}

std::string PriorityToString(UpdateService::Priority priority) {
  switch (priority) {
    case UpdateService::Priority::kUnknown:
      return "UNKNOWN";
    case UpdateService::Priority::kBackground:
      return "BACKGROUND";
    case UpdateService::Priority::kForeground:
      return "FOREGROUND";
  }
}

std::string UpdateStateToString(UpdateService::UpdateState::State state) {
  switch (state) {
    case UpdateService::UpdateState::State::kUnknown:
      return "UNKOWN";
    case UpdateService::UpdateState::State::kNotStarted:
      return "NOT_STARTED";
    case UpdateService::UpdateState::State::kCheckingForUpdates:
      return "CHECKING_FOR_UPDATES";
    case UpdateService::UpdateState::State::kUpdateAvailable:
      return "UPDATE_AVAILABLE";
    case UpdateService::UpdateState::State::kDownloading:
      return "DOWNLOADING";
    case UpdateService::UpdateState::State::kInstalling:
      return "INSTALLING";
    case UpdateService::UpdateState::State::kUpdated:
      return "UPDATED";
    case UpdateService::UpdateState::State::kNoUpdate:
      return "NO_UPDATE";
    case UpdateService::UpdateState::State::kUpdateError:
      return "UPDATE_ERROR";
    case UpdateService::UpdateState::State::kDecompressing:
      return "DECOMPRESSING";
    case UpdateService::UpdateState::State::kPatching:
      return "PATCHING";
  }
}

void SetWorldReadablePermissions(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  // Grant read access to non-admin users.
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  if (!sd) {
    VPLOG(1) << "Failed to read security descriptor for " << path;
    return;
  }
  if (!sd->SetDaclEntry(base::win::Sid(base::win::WellKnownSid::kWorld),
                        base::win::SecurityAccessMode::kGrant,
                        FILE_GENERIC_READ, 0)) {
    VPLOG(1) << "Failed to grant read access to " << path;
    return;
  }
  if (!sd->WriteToFile(path, DACL_SECURITY_INFORMATION)) {
    VPLOG(1) << "Failed to write security descriptor for " << path;
  }
#else
  if (!base::SetPosixFilePermissions(
          path, base::FILE_PERMISSION_READ_BY_USER |
                    base::FILE_PERMISSION_WRITE_BY_USER |
                    base::FILE_PERMISSION_READ_BY_GROUP |
                    base::FILE_PERMISSION_READ_BY_OTHERS)) {
    VPLOG(1) << "Failed to set permissions on " << path;
  }
#endif
}

}  // namespace

void InitHistoryLogging(UpdaterScope updater_scope) {
  constexpr int kMaxFileSizeBytes = 1024 * 1024;  // 1 MiB.
  std::optional<base::FilePath> log_dir = GetInstallDirectory(updater_scope);
  VLOG_IF(1, !log_dir) << "Failed to get history event log file path. "
                          "History event logging will be disabled.";
  if (log_dir) {
    InitHistoryLogging(
        log_dir->Append(FILE_PATH_LITERAL("updater_history.jsonl")),
        kMaxFileSizeBytes);
  }
}

void InitHistoryLogging(const base::FilePath& path,
                        size_t max_file_size_bytes) {
  base::AutoLock lock(GetLoggingLock());

  if (GetLogFile().IsValid()) {
    GetLogFile().Close();
  }

  std::optional<int64_t> file_size = base::GetFileSize(path);
  if (file_size &&
      base::ClampedNumeric<size_t>(*file_size) > max_file_size_bytes) {
    const base::FilePath rotated_path =
        path.AddExtension(FILE_PATH_LITERAL(".old"));
    base::DeleteFile(rotated_path);
    if (!base::Move(path, rotated_path)) {
      VPLOG(1) << "Failed to rotate event history log";
    }
  }

  uint32_t flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND;
#if BUILDFLAG(IS_WIN)
  // base::File opens with FILE_SHARE_READ and FILE_SHARE_WRITE by default if
  // exclusivity flags are not provided.
  flags |= base::File::FLAG_WIN_SHARE_DELETE;
#endif
  GetLogFile() = base::File(path, flags);

  if (GetUpdaterScope() == UpdaterScope::kSystem) {
    SetWorldReadablePermissions(path);
  }
}

std::string GenerateEventId() {
  static std::atomic_uint counter(0);
  return base::NumberToString(counter++);
}

const std::string& GetProcessToken() {
  static const base::NoDestructor<std::string> process_token(
      base::Base64Encode(base::RandBytesAsString(16)));
  return *process_token;
}

void WriteHistoryEvent(base::Value::Dict event) {
  base::AutoLock lock(GetLoggingLock());
  if (!GetLogFile().IsValid()) {
    VLOG(1) << "Failed to write history event: logging not initialized";
    return;
  }

  std::optional<std::string> json = base::WriteJson(event);
  if (!json) {
    VLOG(1) << "Failed to write history event: JSON serialization failed";
    return;
  }
  *json = base::StrCat({*json, "\n"});

  std::optional<size_t> bytes_written =
      GetLogFile().WriteAtCurrentPos(base::as_byte_span(*json));
  if (!bytes_written) {
    VPLOG(1) << "Failed to write history event";
    return;
  }

  if (*bytes_written != json->size()) {
    VLOG(1) << "Failed to write history event: unable to write complete JSON "
               "message. Wrote "
            << *bytes_written << " out of " << json->size() << " bytes";
    return;
  }

  const std::string* event_type = event.FindString("eventType");
  const std::string* bound = event.FindString("bound");
  VLOG(2) << "Emitted a " << (event_type ? *event_type : "(unknown)") << " "
          << (bound ? *bound : "INSTANT") << " event to the history log";
}

base::Value::Dict HistoryEventError::ToDict() const {
  base::Value::Dict dict = base::Value::Dict()
                               .Set("category", category)
                               .Set("code", code)
                               .Set("extracode1", extracode1);
  return dict;
}

InstallStartEvent::InstallStartEvent() = default;
InstallStartEvent::InstallStartEvent(InstallStartEvent&&) = default;
InstallStartEvent& InstallStartEvent::operator=(InstallStartEvent&&) = default;
InstallStartEvent::~InstallStartEvent() = default;

InstallStartEvent& InstallStartEvent::SetAppId(const std::string& app_id) {
  app_id_ = app_id;
  return *this;
}

std::optional<base::Value::Dict> InstallStartEvent::BuildInternal(
    base::Value::Dict event) const {
  if (app_id_.empty()) {
    VLOG(1) << "Failed to build InstallStartEvent, app_id is empty";
    return std::nullopt;
  }
  event.Set("eventType", "INSTALL");
  event.Set("bound", "START");
  event.Set("appId", app_id_);
  return event;
}

InstallEndEvent::InstallEndEvent() = default;
InstallEndEvent::InstallEndEvent(InstallEndEvent&&) = default;
InstallEndEvent& InstallEndEvent::operator=(InstallEndEvent&&) = default;
InstallEndEvent::~InstallEndEvent() = default;

InstallEndEvent& InstallEndEvent::SetVersion(const std::string& version) {
  version_ = version;
  return *this;
}

std::optional<base::Value::Dict> InstallEndEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "INSTALL");
  event.Set("bound", "END");
  if (version_) {
    event.Set("version", *version_);
  }
  return event;
}

UninstallStartEvent::UninstallStartEvent() = default;
UninstallStartEvent::UninstallStartEvent(UninstallStartEvent&&) = default;
UninstallStartEvent& UninstallStartEvent::operator=(UninstallStartEvent&&) =
    default;
UninstallStartEvent::~UninstallStartEvent() = default;

UninstallStartEvent& UninstallStartEvent::SetAppId(const std::string& app_id) {
  app_id_ = app_id;
  return *this;
}

UninstallStartEvent& UninstallStartEvent::SetVersion(
    const std::string& version) {
  version_ = version;
  return *this;
}

UninstallStartEvent& UninstallStartEvent::SetReason(
    UninstallPingReason reason) {
  reason_ = reason;
  return *this;
}

std::optional<base::Value::Dict> UninstallStartEvent::BuildInternal(
    base::Value::Dict event) const {
  if (app_id_.empty()) {
    VLOG(1) << "Failed to build UninstallStartEvent, app_id is empty";
    return std::nullopt;
  }
  if (version_.empty()) {
    VLOG(1) << "Failed to build UninstallStartEvent, version is empty";
    return std::nullopt;
  }
  if (!reason_) {
    VLOG(1) << "Failed to build UninstallStartEvent, reason is empty";
    return std::nullopt;
  }
  event.Set("eventType", "UNINSTALL");
  event.Set("bound", "START");
  event.Set("appId", app_id_);
  event.Set("version", version_);
  event.Set("reason", UninstallPingReasonToString(*reason_));
  return event;
}

UninstallEndEvent::UninstallEndEvent() = default;
UninstallEndEvent::UninstallEndEvent(UninstallEndEvent&&) = default;
UninstallEndEvent& UninstallEndEvent::operator=(UninstallEndEvent&&) = default;
UninstallEndEvent::~UninstallEndEvent() = default;

std::optional<base::Value::Dict> UninstallEndEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "UNINSTALL");
  event.Set("bound", "END");
  return event;
}

QualifyStartEvent::QualifyStartEvent() = default;
QualifyStartEvent::QualifyStartEvent(QualifyStartEvent&&) = default;
QualifyStartEvent& QualifyStartEvent::operator=(QualifyStartEvent&&) = default;
QualifyStartEvent::~QualifyStartEvent() = default;

std::optional<base::Value::Dict> QualifyStartEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "QUALIFY");
  event.Set("bound", "START");
  return event;
}

QualifyEndEvent::QualifyEndEvent() = default;
QualifyEndEvent::QualifyEndEvent(QualifyEndEvent&&) = default;
QualifyEndEvent& QualifyEndEvent::operator=(QualifyEndEvent&&) = default;
QualifyEndEvent::~QualifyEndEvent() = default;

QualifyEndEvent& QualifyEndEvent::SetQualified(bool qualified) {
  qualified_ = qualified;
  return *this;
}

std::optional<base::Value::Dict> QualifyEndEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "QUALIFY");
  event.Set("bound", "END");
  event.Set("qualified", qualified_);
  return event;
}

ActivateStartEvent::ActivateStartEvent() = default;
ActivateStartEvent::ActivateStartEvent(ActivateStartEvent&&) = default;
ActivateStartEvent& ActivateStartEvent::operator=(ActivateStartEvent&&) =
    default;
ActivateStartEvent::~ActivateStartEvent() = default;

std::optional<base::Value::Dict> ActivateStartEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "ACTIVATE");
  event.Set("bound", "START");
  return event;
}

ActivateEndEvent::ActivateEndEvent() = default;
ActivateEndEvent::ActivateEndEvent(ActivateEndEvent&&) = default;
ActivateEndEvent& ActivateEndEvent::operator=(ActivateEndEvent&&) = default;
ActivateEndEvent::~ActivateEndEvent() = default;

ActivateEndEvent& ActivateEndEvent::SetActivated(bool activated) {
  activated_ = activated;
  return *this;
}

std::optional<base::Value::Dict> ActivateEndEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "ACTIVATE");
  event.Set("bound", "END");
  event.Set("activated", activated_);
  return event;
}

PersistedDataEvent::RegisteredApp::RegisteredApp() = default;
PersistedDataEvent::RegisteredApp::RegisteredApp(const RegisteredApp&) =
    default;
PersistedDataEvent::RegisteredApp& PersistedDataEvent::RegisteredApp::operator=(
    const RegisteredApp&) = default;
PersistedDataEvent::RegisteredApp::~RegisteredApp() = default;

PersistedDataEvent::PersistedDataEvent() = default;
PersistedDataEvent::PersistedDataEvent(PersistedDataEvent&&) = default;
PersistedDataEvent& PersistedDataEvent::operator=(PersistedDataEvent&&) =
    default;
PersistedDataEvent::~PersistedDataEvent() = default;

PersistedDataEvent& PersistedDataEvent::SetEulaRequired(bool eula_required) {
  eula_required_ = eula_required;
  return *this;
}

PersistedDataEvent& PersistedDataEvent::SetLastChecked(
    const base::Time& last_checked) {
  last_checked_ = last_checked;
  return *this;
}

PersistedDataEvent& PersistedDataEvent::SetLastStarted(
    const base::Time& last_started) {
  last_started_ = last_started;
  return *this;
}

PersistedDataEvent& PersistedDataEvent::AddRegisteredApp(
    const RegisteredApp& registered_app) {
  registered_apps_.push_back(registered_app);
  return *this;
}

std::optional<base::Value::Dict> PersistedDataEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "PERSISTED_DATA");
  event.Set("bound", "INSTANT");
  event.Set("eulaRequired", eula_required_);
  if (last_checked_) {
    event.Set("lastChecked", base::TimeToValue(*last_checked_));
  }
  if (last_started_) {
    event.Set("lastStarted", base::TimeToValue(*last_started_));
  }
  if (!registered_apps_.empty()) {
    base::Value::List apps;
    for (const auto& app : registered_apps_) {
      base::Value::Dict app_dict;
      app_dict.Set("appId", app.app_id);
      app_dict.Set("version", app.version);
      if (app.cohort) {
        app_dict.Set("cohort", *app.cohort);
      }
      if (app.brand_code) {
        app_dict.Set("brandCode", *app.brand_code);
      }
      apps.Append(std::move(app_dict));
    }
    event.Set("registeredApps", std::move(apps));
  }
  return event;
}

PostRequestStartEvent::PostRequestStartEvent() = default;
PostRequestStartEvent::PostRequestStartEvent(PostRequestStartEvent&&) = default;
PostRequestStartEvent& PostRequestStartEvent::operator=(
    PostRequestStartEvent&&) = default;
PostRequestStartEvent::~PostRequestStartEvent() = default;

PostRequestStartEvent& PostRequestStartEvent::SetRequest(
    const std::string& request) {
  request_ = request;
  return *this;
}

std::optional<base::Value::Dict> PostRequestStartEvent::BuildInternal(
    base::Value::Dict event) const {
  if (request_.empty()) {
    VLOG(1) << "Failed to build PostRequestStartEvent, request is empty";
    return std::nullopt;
  }
  event.Set("eventType", "POST_REQUEST");
  event.Set("bound", "START");
  event.Set("request", request_);
  return event;
}

PostRequestEndEvent::PostRequestEndEvent() = default;
PostRequestEndEvent::PostRequestEndEvent(PostRequestEndEvent&&) = default;
PostRequestEndEvent& PostRequestEndEvent::operator=(PostRequestEndEvent&&) =
    default;
PostRequestEndEvent::~PostRequestEndEvent() = default;

PostRequestEndEvent& PostRequestEndEvent::SetResponse(
    const std::string& response) {
  response_ = response;
  return *this;
}

std::optional<base::Value::Dict> PostRequestEndEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "POST_REQUEST");
  event.Set("bound", "END");
  if (response_) {
    event.Set("response", *response_);
  }
  return event;
}

LoadPolicyStartEvent::LoadPolicyStartEvent() = default;
LoadPolicyStartEvent::LoadPolicyStartEvent(LoadPolicyStartEvent&&) = default;
LoadPolicyStartEvent& LoadPolicyStartEvent::operator=(LoadPolicyStartEvent&&) =
    default;
LoadPolicyStartEvent::~LoadPolicyStartEvent() = default;

std::optional<base::Value::Dict> LoadPolicyStartEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "LOAD_POLICY");
  event.Set("bound", "START");
  return event;
}

LoadPolicyEndEvent::LoadPolicyEndEvent() = default;
LoadPolicyEndEvent::LoadPolicyEndEvent(LoadPolicyEndEvent&&) = default;
LoadPolicyEndEvent& LoadPolicyEndEvent::operator=(LoadPolicyEndEvent&&) =
    default;
LoadPolicyEndEvent::~LoadPolicyEndEvent() = default;

LoadPolicyEndEvent& LoadPolicyEndEvent::SetPolicySet(
    const base::Value::Dict& policy_set) {
  policy_set_ = policy_set.Clone();
  return *this;
}

std::optional<base::Value::Dict> LoadPolicyEndEvent::BuildInternal(
    base::Value::Dict event) const {
  if (policy_set_.empty()) {
    VLOG(1) << "Failed to build LoadPolicyEvent, policy_set is empty";
    return std::nullopt;
  }
  event.Set("eventType", "LOAD_POLICY");
  event.Set("bound", "END");
  event.Set("policySet", policy_set_.Clone());
  return event;
}

UpdateStartEvent::UpdateStartEvent() = default;
UpdateStartEvent::UpdateStartEvent(UpdateStartEvent&&) = default;
UpdateStartEvent& UpdateStartEvent::operator=(UpdateStartEvent&&) = default;
UpdateStartEvent::~UpdateStartEvent() = default;

UpdateStartEvent& UpdateStartEvent::SetAppId(const std::string& app_id) {
  app_id_ = app_id;
  return *this;
}

UpdateStartEvent& UpdateStartEvent::SetPriority(
    UpdateService::Priority priority) {
  priority_ = priority;
  return *this;
}

std::optional<base::Value::Dict> UpdateStartEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "UPDATE");
  event.Set("bound", "START");
  if (app_id_) {
    event.Set("appId", *app_id_);
  }
  if (connection_metered_) {
    event.Set("connectionMetered", *connection_metered_);
  }
  if (priority_) {
    event.Set("priority", PriorityToString(*priority_));
  }
  return event;
}

UpdateEndEvent::UpdateEndEvent() = default;
UpdateEndEvent::UpdateEndEvent(UpdateEndEvent&&) = default;
UpdateEndEvent& UpdateEndEvent::operator=(UpdateEndEvent&&) = default;
UpdateEndEvent::~UpdateEndEvent() = default;

UpdateEndEvent& UpdateEndEvent::SetOutcome(
    UpdateService::UpdateState::State outcome) {
  outcome_ = outcome;
  return *this;
}

UpdateEndEvent& UpdateEndEvent::SetNextVersion(
    const std::string& next_version) {
  next_version_ = next_version;
  return *this;
}

std::optional<base::Value::Dict> UpdateEndEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "UPDATE");
  event.Set("bound", "END");
  if (outcome_) {
    event.Set("outcome", UpdateStateToString(*outcome_));
  }
  if (next_version_) {
    event.Set("nextVersion", *next_version_);
  }
  return event;
}

UpdaterProcessStartEvent::UpdaterProcessStartEvent() = default;
UpdaterProcessStartEvent::UpdaterProcessStartEvent(UpdaterProcessStartEvent&&) =
    default;
UpdaterProcessStartEvent& UpdaterProcessStartEvent::operator=(
    UpdaterProcessStartEvent&&) = default;
UpdaterProcessStartEvent::~UpdaterProcessStartEvent() = default;

UpdaterProcessStartEvent& UpdaterProcessStartEvent::SetCommandLine(
    const std::string& command_line) {
  command_line_ = command_line;
  return *this;
}

UpdaterProcessStartEvent& UpdaterProcessStartEvent::SetTimestamp(
    const base::Time& timestamp) {
  timestamp_ = timestamp;
  return *this;
}

UpdaterProcessStartEvent& UpdaterProcessStartEvent::SetUpdaterVersion(
    const std::string& updater_version) {
  updater_version_ = updater_version;
  return *this;
}

UpdaterProcessStartEvent& UpdaterProcessStartEvent::SetScope(
    UpdaterScope scope) {
  scope_ = scope;
  return *this;
}

UpdaterProcessStartEvent& UpdaterProcessStartEvent::SetOsPlatform(
    const std::string& os_platform) {
  os_platform_ = os_platform;
  return *this;
}

UpdaterProcessStartEvent& UpdaterProcessStartEvent::SetOsVersion(
    const std::string& os_version) {
  os_version_ = os_version;
  return *this;
}

UpdaterProcessStartEvent& UpdaterProcessStartEvent::SetOsArchitecture(
    const std::string& os_architecture) {
  os_architecture_ = os_architecture;
  return *this;
}

UpdaterProcessStartEvent& UpdaterProcessStartEvent::SetUpdaterArchitecture(
    const std::string& updater_architecture) {
  updater_architecture_ = updater_architecture;
  return *this;
}

UpdaterProcessStartEvent& UpdaterProcessStartEvent::SetParentPid(
    int parent_pid) {
  parent_pid_ = parent_pid;
  return *this;
}

std::optional<base::Value::Dict> UpdaterProcessStartEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "UPDATER_PROCESS");
  event.Set("bound", "START");
  if (command_line_) {
    event.Set("commandLine", *command_line_);
  }
  if (timestamp_) {
    event.Set("timestamp", base::TimeToValue(*timestamp_));
  }
  if (updater_version_) {
    event.Set("updaterVersion", *updater_version_);
  }
  if (scope_) {
    event.Set("scope", base::ToUpperASCII(UpdaterScopeToString(*scope_)));
  }
  if (os_platform_) {
    event.Set("osPlatform", *os_platform_);
  }
  if (os_version_) {
    event.Set("osVersion", *os_version_);
  }
  if (os_architecture_) {
    event.Set("osArchitecture", *os_architecture_);
  }
  if (updater_architecture_) {
    event.Set("updaterArchitecture", *updater_architecture_);
  }
  if (parent_pid_) {
    event.Set("parentPid", *parent_pid_);
  }
  return event;
}

UpdaterProcessEndEvent::UpdaterProcessEndEvent() = default;
UpdaterProcessEndEvent::UpdaterProcessEndEvent(UpdaterProcessEndEvent&&) =
    default;
UpdaterProcessEndEvent& UpdaterProcessEndEvent::operator=(
    UpdaterProcessEndEvent&&) = default;
UpdaterProcessEndEvent::~UpdaterProcessEndEvent() = default;

UpdaterProcessEndEvent& UpdaterProcessEndEvent::SetExitCode(int exit_code) {
  exit_code_ = exit_code;
  return *this;
}

std::optional<base::Value::Dict> UpdaterProcessEndEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "UPDATER_PROCESS");
  event.Set("bound", "END");
  if (exit_code_) {
    event.Set("exitCode", *exit_code_);
  }
  return event;
}

AppCommandStartEvent::AppCommandStartEvent() = default;
AppCommandStartEvent::AppCommandStartEvent(AppCommandStartEvent&&) = default;
AppCommandStartEvent& AppCommandStartEvent::operator=(AppCommandStartEvent&&) =
    default;
AppCommandStartEvent::~AppCommandStartEvent() = default;

AppCommandStartEvent& AppCommandStartEvent::SetAppId(
    const std::string& app_id) {
  app_id_ = app_id;
  return *this;
}

AppCommandStartEvent& AppCommandStartEvent::SetCommandLine(
    const std::string& command_line) {
  command_line_ = command_line;
  return *this;
}

std::optional<base::Value::Dict> AppCommandStartEvent::BuildInternal(
    base::Value::Dict event) const {
  if (app_id_.empty()) {
    VLOG(1) << "Failed to build AppCommandStartEvent, app_id is empty";
    return std::nullopt;
  }
  event.Set("eventType", "APP_COMMAND");
  event.Set("bound", "START");
  event.Set("appId", app_id_);
  if (command_line_) {
    event.Set("commandLine", *command_line_);
  }
  return event;
}

AppCommandEndEvent::AppCommandEndEvent() = default;
AppCommandEndEvent::AppCommandEndEvent(AppCommandEndEvent&&) = default;
AppCommandEndEvent& AppCommandEndEvent::operator=(AppCommandEndEvent&&) =
    default;
AppCommandEndEvent::~AppCommandEndEvent() = default;

AppCommandEndEvent& AppCommandEndEvent::SetExitCode(int exit_code) {
  exit_code_ = exit_code;
  return *this;
}

AppCommandEndEvent& AppCommandEndEvent::SetOutput(const std::string& output) {
  output_ = output;
  return *this;
}

std::optional<base::Value::Dict> AppCommandEndEvent::BuildInternal(
    base::Value::Dict event) const {
  event.Set("eventType", "APP_COMMAND");
  event.Set("bound", "END");
  if (exit_code_) {
    event.Set("exitCode", *exit_code_);
  }
  if (output_) {
    event.Set("output", *output_);
  }
  return event;
}

}  // namespace updater
