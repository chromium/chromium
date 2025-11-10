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

}  // namespace updater

#endif  // CHROME_UPDATER_EVENT_HISTORY_H_
