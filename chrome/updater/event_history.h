// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EVENT_HISTORY_H_
#define CHROME_UPDATER_EVENT_HISTORY_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"

// This API provides mechanisms to work with updater events, which are recorded
// in the history log. The API implements the schema defined in
// //docs/updater/history_log.md.
//
// Events are created in the updater using a fluent Builder pattern and can be
// serialized to `base::Value::Dict` objects, which directly correspond to the
// JSON objects described in the schema.
//
// Event objects are immutable upon construction and are thread-safe. Builders
// are not thread-safe.
//
// Example API usage:
//
//    updater::InstallStartEvent::Builder()
//        .SetEventId("custom-event-id-123")
//        .SetAppId("my-app-id")
//        .AddError({.category = 1, .code = 2, .extracode1 = 3})
//        .Write();

namespace updater {

// Must be called before any events are written to initialize global logging
// state. A log file at `path` is created if one does not already exist.
//
// For system-scoped installations, this operation updates the file permissions
// / security descriptor for `path` to allow any user to read the log. This is
// essential for non-privileged presentation layers (e.g. Chrome) to load the
// data.
//
// Under normal circumstances this function should be called once per process,
// however it is safe to call multiple times (e.g. to redirect all future
// logging to a different file). Initialization is thread-safe, even if
// reinitializing.
//
// If the log file is larger than `max_file_size_bytes` at the time of
// initialization, it is rotated to `path`.old, deleting the previous "old" file
// if it exists.
void InitHistoryLogging(const base::FilePath& path, size_t max_file_size_bytes);

// Generates an event ID unique to this process. An ID is required for all
// events. The same ID may be used in multiple events, e.g. to link START and
// END records.
std::string GenerateEventId();

// Returns a process-specific token which can be used to discriminate between
// processes with the same PID in cross-process logs due to OS-reuse.
const std::string& GetProcessToken();

// Implementation detail which must be exposed for `HistoryEventBuilder`.
// Users of the API should use the `Write` method on the event class.
void WriteHistoryEvent(const base::Value::Dict& event);

struct HistoryEventError {
  int category = 0;
  int code = 0;
  int extracode1 = 0;

  // Serializes the error to a `base::Value::Dict`.
  base::Value::Dict ToDict() const;
};

// A mixin template for event builders, providing common functionality like
// setting event ID and adding errors.
template <typename T>
class HistoryEventBuilder {
 public:
  T& SetEventId(const std::string& event_id) {
    event_id_ = event_id;
    return static_cast<T&>(*this);
  }

  T& AddError(const HistoryEventError& error) {
    errors_.push_back(error);
    return static_cast<T&>(*this);
  }

  // Builds the base::Value::Dict representation of an event. Returns
  // `std::nullopt` if a required common field is missing.
  std::optional<base::Value::Dict> Build() const {
    if (event_id_.empty()) {
      VLOG(1) << "Failed to build common fields: event_id is empty";
      return std::nullopt;
    }
    base::Value::Dict dict =
        base::Value::Dict()
            .Set("eventId", event_id_)
            .Set("deviceUptime",
                 base::TimeDeltaToValue(base::SysInfo::Uptime()))
            .Set("pid", static_cast<int>(base::Process::Current().Pid()))
            .Set("processToken", GetProcessToken());
    if (!errors_.empty()) {
      base::Value::List errors;
      for (const auto& error : errors_) {
        errors.Append(error.ToDict());
      }
      dict.Set("errors", std::move(errors));
    }
    return BuildInternal(std::move(dict));
  }

  // Builds, serializes, and writes the event to storage. Errors are VLOGed. A
  // failure to build the event indicates a programming error; a dump is created
  // without crashing.
  void Write() {
    std::optional<base::Value::Dict> event = Build();
    if (!event) {
      base::debug::DumpWithoutCrashing();
      return;
    }
    WriteHistoryEvent(*event);
  }

 protected:
  HistoryEventBuilder() = default;
  virtual ~HistoryEventBuilder() = default;

  // To be implemented by concrete event types. Implementors should add
  // event-specific fields to the `event` dict.
  virtual std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const = 0;

  std::string event_id_;
  std::vector<HistoryEventError> errors_;
};

class InstallStartEvent : public HistoryEventBuilder<InstallStartEvent> {
 public:
  InstallStartEvent();
  ~InstallStartEvent() override;

  InstallStartEvent& SetAppId(const std::string& app_id);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::string app_id_;
};

class InstallEndEvent : public HistoryEventBuilder<InstallEndEvent> {
 public:
  InstallEndEvent();
  ~InstallEndEvent() override;

  InstallEndEvent& SetVersion(const std::string& version);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<std::string> version_;
};

class UninstallStartEvent : public HistoryEventBuilder<UninstallStartEvent> {
 public:
  UninstallStartEvent();
  ~UninstallStartEvent() override;

  UninstallStartEvent& SetAppId(const std::string& app_id);
  UninstallStartEvent& SetVersion(const std::string& version);
  UninstallStartEvent& SetReason(UninstallPingReason reason);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::string app_id_;
  std::string version_;
  std::optional<UninstallPingReason> reason_;
};

class UninstallEndEvent : public HistoryEventBuilder<UninstallEndEvent> {
 public:
  UninstallEndEvent();
  ~UninstallEndEvent() override;

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;
};

class QualifyStartEvent : public HistoryEventBuilder<QualifyStartEvent> {
 public:
  QualifyStartEvent();
  ~QualifyStartEvent() override;

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;
};

class QualifyEndEvent : public HistoryEventBuilder<QualifyEndEvent> {
 public:
  QualifyEndEvent();
  ~QualifyEndEvent() override;

  QualifyEndEvent& SetQualified(bool qualified);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  bool qualified_ = false;
};

class ActivateStartEvent : public HistoryEventBuilder<ActivateStartEvent> {
 public:
  ActivateStartEvent();
  ~ActivateStartEvent() override;

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;
};

class ActivateEndEvent : public HistoryEventBuilder<ActivateEndEvent> {
 public:
  ActivateEndEvent();
  ~ActivateEndEvent() override;

  ActivateEndEvent& SetActivated(bool activated);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  bool activated_ = false;
};

class PersistedDataEvent : public HistoryEventBuilder<PersistedDataEvent> {
 public:
  struct RegisteredApp {
    RegisteredApp();
    RegisteredApp(const RegisteredApp&);
    RegisteredApp& operator=(const RegisteredApp&);
    ~RegisteredApp();
    std::string app_id;
    std::string version;
    std::optional<std::string> cohort;
    std::optional<std::string> brand_code;
  };

  PersistedDataEvent();
  ~PersistedDataEvent() override;

  PersistedDataEvent& SetEulaRequired(bool eula_required);
  PersistedDataEvent& SetLastChecked(const base::Time& last_checked);
  PersistedDataEvent& SetLastStarted(const base::Time& last_started);
  PersistedDataEvent& AddRegisteredApp(const RegisteredApp& registered_app);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  bool eula_required_ = false;
  std::optional<base::Time> last_checked_;
  std::optional<base::Time> last_started_;
  std::vector<RegisteredApp> registered_apps_;
};

class OmahaRequestStartEvent
    : public HistoryEventBuilder<OmahaRequestStartEvent> {
 public:
  OmahaRequestStartEvent();
  ~OmahaRequestStartEvent() override;

  OmahaRequestStartEvent& SetRequest(const std::string& request);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::string request_;
};

class OmahaRequestEndEvent : public HistoryEventBuilder<OmahaRequestEndEvent> {
 public:
  OmahaRequestEndEvent();
  ~OmahaRequestEndEvent() override;

  OmahaRequestEndEvent& SetResponse(const std::string& response);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::string response_;
};

class UpdateStartEvent : public HistoryEventBuilder<UpdateStartEvent> {
 public:
  UpdateStartEvent();
  ~UpdateStartEvent() override;

  UpdateStartEvent& SetAppId(const std::string& app_id);
  UpdateStartEvent& SetConnectionMetered(bool connection_metered);
  UpdateStartEvent& SetPriority(UpdateService::Priority priority);
  UpdateStartEvent& SetInstallSource(const std::string& install_source);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<std::string> app_id_;
  std::optional<bool> connection_metered_;
  std::optional<UpdateService::Priority> priority_;
  std::optional<std::string> install_source_;
};

class UpdateEndEvent : public HistoryEventBuilder<UpdateEndEvent> {
 public:
  UpdateEndEvent();
  ~UpdateEndEvent() override;

  UpdateEndEvent& SetOutcome(UpdateService::UpdateState::State outcome);
  UpdateEndEvent& SetVersion(const std::string& version);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<UpdateService::UpdateState::State> outcome_;
  std::optional<std::string> version_;
};

class UpdaterProcessStartEvent
    : public HistoryEventBuilder<UpdaterProcessStartEvent> {
 public:
  UpdaterProcessStartEvent();
  ~UpdaterProcessStartEvent() override;

  UpdaterProcessStartEvent& SetCommandLine(const std::string& command_line);
  UpdaterProcessStartEvent& SetTimestamp(const base::Time& timestamp);
  UpdaterProcessStartEvent& SetUpdaterVersion(
      const std::string& updater_version);
  UpdaterProcessStartEvent& SetScope(UpdaterScope scope);
  UpdaterProcessStartEvent& SetOsPlatform(const std::string& os_platform);
  UpdaterProcessStartEvent& SetOsVersion(const std::string& os_version);
  UpdaterProcessStartEvent& SetOsArchitecture(
      const std::string& os_architecture);
  UpdaterProcessStartEvent& SetUpdaterArchitecture(
      const std::string& updater_architecture);
  UpdaterProcessStartEvent& SetParentPid(int parent_pid);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<std::string> command_line_;
  std::optional<base::Time> timestamp_;
  std::optional<std::string> updater_version_;
  std::optional<UpdaterScope> scope_;
  std::optional<std::string> os_platform_;
  std::optional<std::string> os_version_;
  std::optional<std::string> os_architecture_;
  std::optional<std::string> updater_architecture_;
  std::optional<int> parent_pid_;
};

class UpdaterProcessEndEvent
    : public HistoryEventBuilder<UpdaterProcessEndEvent> {
 public:
  UpdaterProcessEndEvent();
  ~UpdaterProcessEndEvent() override;

  UpdaterProcessEndEvent& SetExitCode(int exit_code);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<int> exit_code_;
};

class AppCommandStartEvent : public HistoryEventBuilder<AppCommandStartEvent> {
 public:
  AppCommandStartEvent();
  ~AppCommandStartEvent() override;

  AppCommandStartEvent& SetAppId(const std::string& app_id);
  AppCommandStartEvent& SetCommandLine(const std::string& command_line);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::string app_id_;
  std::optional<std::string> command_line_;
};

class AppCommandEndEvent : public HistoryEventBuilder<AppCommandEndEvent> {
 public:
  AppCommandEndEvent();
  ~AppCommandEndEvent() override;

  AppCommandEndEvent& SetExitCode(int exit_code);
  AppCommandEndEvent& SetOutput(const std::string& output);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<int> exit_code_;
  std::optional<std::string> output_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EVENT_HISTORY_H_
