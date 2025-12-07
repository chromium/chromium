// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EVENT_HISTORY_H_
#define CHROME_UPDATER_EVENT_HISTORY_H_

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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

// Alias for the above using defaults for the updater.
void InitHistoryLogging(UpdaterScope updater_scope);

// Generates an event ID unique to this process. An ID is required for all
// events. The same ID may be used in multiple events, e.g. to link START and
// END records.
std::string GenerateEventId();

// Returns a process-specific token which can be used to discriminate between
// processes with the same PID in cross-process logs due to OS-reuse.
const std::string& GetProcessToken();

// Implementation detail which must be exposed for `HistoryEventBuilder`.
// Users of the API should use the `Write` method on the event class.
void WriteHistoryEvent(base::Value::Dict event);

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
  // Events are assigned a process-unique identifier on construction (see
  // `GenerateEventId`), though this may be overridden via this setter.
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
  // without crashing. If calling from a sequence that disallows blocking, use
  // `WriteAsync`.
  void Write() {
    std::optional<base::Value::Dict> event = Build();
    if (!event) {
      base::debug::DumpWithoutCrashing();
      return;
    }
    WriteHistoryEvent(*std::move(event));
  }

  // Same as `Write` except that blocking IO is posted to the thread pool. This
  // is suitable from calling from sequences which may not block (e.g. the main
  // sequence).
  void WriteAsync() {
    std::optional<base::Value::Dict> event = Build();
    if (!event) {
      base::debug::DumpWithoutCrashing();
      return;
    }
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&WriteHistoryEvent, *std::move(event)));
  }

  // Performs `WriteAsync` and returns the corresponding END event builder for
  // this START event with the same event id.
  auto WriteAsyncAndReturnEndEvent()
    requires requires {
      typename T::EndEventType;
      std::default_initializable<typename T::EndEventType>;
      std::derived_from<typename T::EndEventType,
                        HistoryEventBuilder<typename T::EndEventType>>;
    }
  {
    WriteAsync();
    typename T::EndEventType end_event;
    end_event.SetEventId(event_id_);
    return end_event;
  }

 protected:
  HistoryEventBuilder() = default;
  virtual ~HistoryEventBuilder() = default;

  // To be implemented by concrete event types. Implementors should add
  // event-specific fields to the `event` dict.
  virtual std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const = 0;

  std::string event_id_ = GenerateEventId();
  std::vector<HistoryEventError> errors_;
};

class InstallEndEvent : public HistoryEventBuilder<InstallEndEvent> {
 public:
  InstallEndEvent();
  InstallEndEvent(InstallEndEvent&&);
  InstallEndEvent& operator=(InstallEndEvent&&);
  InstallEndEvent(const InstallEndEvent&) = delete;
  InstallEndEvent& operator=(const InstallEndEvent&) = delete;
  ~InstallEndEvent() override;

  InstallEndEvent& SetVersion(const std::string& version);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<std::string> version_;
};

class InstallStartEvent : public HistoryEventBuilder<InstallStartEvent> {
 public:
  using EndEventType = InstallEndEvent;

  InstallStartEvent();
  InstallStartEvent(InstallStartEvent&&);
  InstallStartEvent& operator=(InstallStartEvent&&);
  InstallStartEvent(const InstallStartEvent&) = delete;
  InstallStartEvent& operator=(const InstallStartEvent&) = delete;
  ~InstallStartEvent() override;

  InstallStartEvent& SetAppId(const std::string& app_id);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::string app_id_;
};

class UninstallEndEvent : public HistoryEventBuilder<UninstallEndEvent> {
 public:
  UninstallEndEvent();
  UninstallEndEvent(UninstallEndEvent&&);
  UninstallEndEvent& operator=(UninstallEndEvent&&);
  UninstallEndEvent(const UninstallEndEvent&) = delete;
  UninstallEndEvent& operator=(const UninstallEndEvent&) = delete;
  ~UninstallEndEvent() override;

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;
};

class UninstallStartEvent : public HistoryEventBuilder<UninstallStartEvent> {
 public:
  using EndEventType = UninstallEndEvent;

  UninstallStartEvent();
  UninstallStartEvent(UninstallStartEvent&&);
  UninstallStartEvent& operator=(UninstallStartEvent&&);
  UninstallStartEvent(const UninstallStartEvent&) = delete;
  UninstallStartEvent& operator=(const UninstallStartEvent&) = delete;
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

class QualifyEndEvent : public HistoryEventBuilder<QualifyEndEvent> {
 public:
  QualifyEndEvent();
  QualifyEndEvent(QualifyEndEvent&&);
  QualifyEndEvent& operator=(QualifyEndEvent&&);
  QualifyEndEvent(const QualifyEndEvent&) = delete;
  QualifyEndEvent& operator=(const QualifyEndEvent&) = delete;
  ~QualifyEndEvent() override;

  QualifyEndEvent& SetQualified(bool qualified);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  bool qualified_ = false;
};

class QualifyStartEvent : public HistoryEventBuilder<QualifyStartEvent> {
 public:
  using EndEventType = QualifyEndEvent;

  QualifyStartEvent();
  QualifyStartEvent(QualifyStartEvent&&);
  QualifyStartEvent& operator=(QualifyStartEvent&&);
  QualifyStartEvent(const QualifyStartEvent&) = delete;
  QualifyStartEvent& operator=(const QualifyStartEvent&) = delete;
  ~QualifyStartEvent() override;

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;
};

class ActivateEndEvent : public HistoryEventBuilder<ActivateEndEvent> {
 public:
  ActivateEndEvent();
  ActivateEndEvent(ActivateEndEvent&&);
  ActivateEndEvent& operator=(ActivateEndEvent&&);
  ActivateEndEvent(const ActivateEndEvent&) = delete;
  ActivateEndEvent& operator=(const ActivateEndEvent&) = delete;
  ~ActivateEndEvent() override;

  ActivateEndEvent& SetActivated(bool activated);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  bool activated_ = false;
};

class ActivateStartEvent : public HistoryEventBuilder<ActivateStartEvent> {
 public:
  using EndEventType = ActivateEndEvent;

  ActivateStartEvent();
  ActivateStartEvent(ActivateStartEvent&&);
  ActivateStartEvent& operator=(ActivateStartEvent&&);
  ActivateStartEvent(const ActivateStartEvent&) = delete;
  ActivateStartEvent& operator=(const ActivateStartEvent&) = delete;
  ~ActivateStartEvent() override;

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;
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
  PersistedDataEvent(PersistedDataEvent&&);
  PersistedDataEvent& operator=(PersistedDataEvent&&);
  PersistedDataEvent(const PersistedDataEvent&) = delete;
  PersistedDataEvent& operator=(const PersistedDataEvent&) = delete;
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

class PostRequestEndEvent : public HistoryEventBuilder<PostRequestEndEvent> {
 public:
  PostRequestEndEvent();
  PostRequestEndEvent(PostRequestEndEvent&&);
  PostRequestEndEvent& operator=(PostRequestEndEvent&&);
  PostRequestEndEvent(const PostRequestEndEvent&) = delete;
  PostRequestEndEvent& operator=(const PostRequestEndEvent&) = delete;
  ~PostRequestEndEvent() override;

  PostRequestEndEvent& SetResponse(const std::string& response);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<std::string> response_;
};

class PostRequestStartEvent
    : public HistoryEventBuilder<PostRequestStartEvent> {
 public:
  using EndEventType = PostRequestEndEvent;

  PostRequestStartEvent();
  PostRequestStartEvent(PostRequestStartEvent&&);
  PostRequestStartEvent& operator=(PostRequestStartEvent&&);
  PostRequestStartEvent(const PostRequestStartEvent&) = delete;
  PostRequestStartEvent& operator=(const PostRequestStartEvent&) = delete;
  ~PostRequestStartEvent() override;

  PostRequestStartEvent& SetRequest(const std::string& request);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::string request_;
};

class LoadPolicyEndEvent : public HistoryEventBuilder<LoadPolicyEndEvent> {
 public:
  LoadPolicyEndEvent();
  LoadPolicyEndEvent(LoadPolicyEndEvent&&);
  LoadPolicyEndEvent& operator=(LoadPolicyEndEvent&&);
  LoadPolicyEndEvent(const LoadPolicyEndEvent&) = delete;
  LoadPolicyEndEvent& operator=(const LoadPolicyEndEvent&) = delete;
  ~LoadPolicyEndEvent() override;

  // Sets the pre-built policy set dictionary as generated by
  // PolicyService::GetAllPolicies.
  LoadPolicyEndEvent& SetPolicySet(const base::Value::Dict& policy_set);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  base::Value::Dict policy_set_;
};

class LoadPolicyStartEvent : public HistoryEventBuilder<LoadPolicyStartEvent> {
 public:
  using EndEventType = LoadPolicyEndEvent;

  LoadPolicyStartEvent();
  LoadPolicyStartEvent(LoadPolicyStartEvent&&);
  LoadPolicyStartEvent& operator=(LoadPolicyStartEvent&&);
  LoadPolicyStartEvent(const LoadPolicyStartEvent&) = delete;
  LoadPolicyStartEvent& operator=(const LoadPolicyStartEvent&) = delete;
  ~LoadPolicyStartEvent() override;

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;
};

class UpdateEndEvent : public HistoryEventBuilder<UpdateEndEvent> {
 public:
  UpdateEndEvent();
  UpdateEndEvent(UpdateEndEvent&&);
  UpdateEndEvent& operator=(UpdateEndEvent&&);
  UpdateEndEvent(const UpdateEndEvent&) = delete;
  UpdateEndEvent& operator=(const UpdateEndEvent&) = delete;
  ~UpdateEndEvent() override;

  UpdateEndEvent& SetOutcome(UpdateService::UpdateState::State outcome);
  UpdateEndEvent& SetNextVersion(const std::string& next_version);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<UpdateService::UpdateState::State> outcome_;
  std::optional<std::string> next_version_;
};

class UpdateStartEvent : public HistoryEventBuilder<UpdateStartEvent> {
 public:
  using EndEventType = UpdateEndEvent;

  UpdateStartEvent();
  UpdateStartEvent(UpdateStartEvent&&);
  UpdateStartEvent& operator=(UpdateStartEvent&&);
  UpdateStartEvent(const UpdateStartEvent&) = delete;
  UpdateStartEvent& operator=(const UpdateStartEvent&) = delete;
  ~UpdateStartEvent() override;

  UpdateStartEvent& SetAppId(const std::string& app_id);
  UpdateStartEvent& SetConnectionMetered(bool connection_metered);
  UpdateStartEvent& SetPriority(UpdateService::Priority priority);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<std::string> app_id_;
  std::optional<bool> connection_metered_;
  std::optional<UpdateService::Priority> priority_;
  std::optional<std::string> install_source_;
};

class UpdaterProcessEndEvent
    : public HistoryEventBuilder<UpdaterProcessEndEvent> {
 public:
  UpdaterProcessEndEvent();
  UpdaterProcessEndEvent(UpdaterProcessEndEvent&&);
  UpdaterProcessEndEvent& operator=(UpdaterProcessEndEvent&&);
  UpdaterProcessEndEvent(const UpdaterProcessEndEvent&) = delete;
  UpdaterProcessEndEvent& operator=(const UpdaterProcessEndEvent&) = delete;
  ~UpdaterProcessEndEvent() override;

  UpdaterProcessEndEvent& SetExitCode(int exit_code);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<int> exit_code_;
};

class UpdaterProcessStartEvent
    : public HistoryEventBuilder<UpdaterProcessStartEvent> {
 public:
  using EndEventType = UpdaterProcessEndEvent;

  UpdaterProcessStartEvent();
  UpdaterProcessStartEvent(UpdaterProcessStartEvent&&);
  UpdaterProcessStartEvent& operator=(UpdaterProcessStartEvent&&);
  UpdaterProcessStartEvent(const UpdaterProcessStartEvent&) = delete;
  UpdaterProcessStartEvent& operator=(const UpdaterProcessStartEvent&) = delete;
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

class AppCommandEndEvent : public HistoryEventBuilder<AppCommandEndEvent> {
 public:
  AppCommandEndEvent();
  AppCommandEndEvent(AppCommandEndEvent&&);
  AppCommandEndEvent& operator=(AppCommandEndEvent&&);
  AppCommandEndEvent(const AppCommandEndEvent&) = delete;
  AppCommandEndEvent& operator=(const AppCommandEndEvent&) = delete;
  ~AppCommandEndEvent() override;

  AppCommandEndEvent& SetExitCode(int exit_code);
  AppCommandEndEvent& SetOutput(const std::string& output);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::optional<int> exit_code_;
  std::optional<std::string> output_;
};

class AppCommandStartEvent : public HistoryEventBuilder<AppCommandStartEvent> {
 public:
  using EndEventType = AppCommandEndEvent;

  AppCommandStartEvent();
  AppCommandStartEvent(AppCommandStartEvent&&);
  AppCommandStartEvent& operator=(AppCommandStartEvent&&);
  AppCommandStartEvent(const AppCommandStartEvent&) = delete;
  AppCommandStartEvent& operator=(const AppCommandStartEvent&) = delete;
  ~AppCommandStartEvent() override;

  AppCommandStartEvent& SetAppId(const std::string& app_id);
  AppCommandStartEvent& SetCommandLine(const std::string& command_line);

 private:
  std::optional<base::Value::Dict> BuildInternal(
      base::Value::Dict event) const override;

  std::string app_id_;
  std::optional<std::string> command_line_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EVENT_HISTORY_H_
