// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EVENT_HISTORY_H_
#define CHROME_UPDATER_EVENT_HISTORY_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/process/process.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
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
// Usage examples:
//
// Create an event using its builder:
//
//    std::unique_ptr<updater::InstallStartEvent> event =
//        updater::InstallStartEvent::Builder()
//            .SetEventId("custom-event-id-123")
//            .SetAppId("my-app-id")
//            .AddError({.category = 1, .code = 2, .extracode1 = 3})
//            .Build();
//
// Serialize an event to a dictionary:
//
//    base::Value::Dict event_dict = event->ToDict();

namespace updater {

// Generates an event ID unique to this process. An ID is required for all
// events. The same ID may be used in multiple events, e.g. to link START and
// END records.
std::string GenerateEventId();

// Returns a process-specific token which can be used to discriminate between
// processes with the same PID in cross-process logs due to OS-reuse.
const std::string& GetProcessToken();

class Event {
 public:
  // Indicates if the record marks the beginning, end, or an instantaneous
  // event.
  enum class Bound { kStart, kEnd, kInstant };

  struct Error;

  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;
  virtual ~Event();

  // Common properties, as defined in docs/updater/history_log.md.
  const std::string& event_type() const { return event_type_; }
  const std::string& event_id() const { return event_id_; }
  base::TimeDelta device_uptime() const { return device_uptime_; }
  int pid() const { return pid_; }
  const std::string& process_token() const { return process_token_; }
  Bound bound() const { return bound_; }
  const std::vector<Error>& errors() const { return errors_; }

  // Serializes the event to a `base::Value::Dict` according to the schema.
  base::Value::Dict ToDict() const;

 protected:
  struct CommonFields;

  // A mixin template for event builders, providing common functionality like
  // setting event ID and adding errors. See `BuilderMixin` for more details.
  template <typename T>
  class BuilderMixin;

  Event(const std::string& event_type,
        Bound bound,
        const CommonFields& common_fields);

  // Derived event types implement this method to add event-specific fields to
  // the serialized dict.
  virtual void ToDictInternal(base::Value::Dict& dict) const = 0;

 private:
  const std::string event_type_;
  const Bound bound_;
  const std::string event_id_;
  const base::TimeDelta device_uptime_;
  const int pid_;
  const std::string process_token_;
  const std::vector<Error> errors_;
};

// An updater error triplet.
struct Event::Error {
  int category = 0;
  int code = 0;
  int extracode1 = 0;

  // Serializes the error to a `base::Value::Dict`.
  base::Value::Dict ToDict() const;
};

// Bundles properties common to all events for implementation brevity.
struct Event::CommonFields {
  CommonFields(std::string event_id,
               base::TimeDelta device_uptime,
               int pid,
               const std::string& process_token,
               const std::vector<Error>& errors);
  CommonFields(const CommonFields&);
  CommonFields& operator=(const CommonFields&);
  CommonFields(CommonFields&&);
  CommonFields& operator=(CommonFields&&);
  ~CommonFields();

  std::string event_id;
  base::TimeDelta device_uptime;
  int pid;
  std::string process_token;
  std::vector<Error> errors;
};

template <typename T>
class Event::BuilderMixin {
 public:
  T& SetEventId(const std::string& event_id) {
    event_id_ = event_id;
    return static_cast<T&>(*this);
  }

  T& AddError(const Event::Error& error) {
    errors_.push_back(error);
    return static_cast<T&>(*this);
  }

 protected:
  BuilderMixin() = default;
  virtual ~BuilderMixin() = default;

  // Builds the common fields for the event. Returns `std::nullopt` if a
  // required common field is missing.
  std::optional<CommonFields> BuildCommonFields() const {
    if (event_id_.empty()) {
      VLOG(1) << "Failed to build common fields: event_id is empty";
      return std::nullopt;
    }
    return CommonFields(event_id_, base::SysInfo::Uptime(),
                        base::Process::Current().Pid(), GetProcessToken(),
                        errors_);
  }

  std::string event_id_;
  std::vector<Event::Error> errors_;
};

class InstallStartEvent : public Event {
 public:
  class Builder;
  InstallStartEvent(const InstallStartEvent&) = delete;
  InstallStartEvent& operator=(const InstallStartEvent&) = delete;
  ~InstallStartEvent() override;

  // The application id for which installation was intended.
  const std::string& app_id() const { return app_id_; }

 private:
  InstallStartEvent(const CommonFields& common_fields,
                    const std::string& app_id);

  void ToDictInternal(base::Value::Dict& dict) const override;

  const std::string app_id_;
};

class InstallStartEvent::Builder : public Event::BuilderMixin<Builder> {
 public:
  Builder();
  ~Builder() override;

  Builder& SetAppId(const std::string& app_id);

  std::unique_ptr<InstallStartEvent> Build() const;

 private:
  std::string app_id_;
};

class InstallEndEvent : public Event {
 public:
  class Builder;
  InstallEndEvent(const InstallEndEvent&) = delete;
  InstallEndEvent& operator=(const InstallEndEvent&) = delete;
  ~InstallEndEvent() override;

  // Returns the version of the application which attempted installation.
  std::optional<std::string> version() const { return version_; }

 private:
  InstallEndEvent(const CommonFields& common_fields,
                  std::optional<std::string> version);

  void ToDictInternal(base::Value::Dict& dict) const override;

  const std::optional<std::string> version_;
};

class InstallEndEvent::Builder : public Event::BuilderMixin<Builder> {
 public:
  Builder();
  ~Builder() override;

  Builder& SetVersion(const std::string& version);

  std::unique_ptr<InstallEndEvent> Build() const;

 private:
  std::optional<std::string> version_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EVENT_HISTORY_H_
