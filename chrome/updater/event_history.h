// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EVENT_HISTORY_H_
#define CHROME_UPDATER_EVENT_HISTORY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"

// This API provides mechanisms to work with updater events, which are recorded
// in the history log. The API implements the schema defined in
// //docs/updater/history_log.md.
//
// Event objects are immutable upon construction and are thread-safe.

namespace updater {

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

 protected:
  struct CommonFields;

  Event(const std::string& event_type,
        Bound bound,
        const CommonFields& common_fields);

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

class InstallStartEvent : public Event {
 public:
  InstallStartEvent(const InstallStartEvent&) = delete;
  InstallStartEvent& operator=(const InstallStartEvent&) = delete;
  ~InstallStartEvent() override;

  // The application id for which installation was intended.
  const std::string& app_id() const { return app_id_; }

 private:
  InstallStartEvent(const CommonFields& common_fields,
                    const std::string& app_id);

  const std::string app_id_;
};

class InstallEndEvent : public Event {
 public:
  InstallEndEvent(const InstallEndEvent&) = delete;
  InstallEndEvent& operator=(const InstallEndEvent&) = delete;
  ~InstallEndEvent() override;

  // Returns the version of the application which attempted installation.
  std::optional<std::string> version() const { return version_; }

 private:
  InstallEndEvent(const CommonFields& common_fields,
                  std::optional<std::string> version);

  const std::optional<std::string> version_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EVENT_HISTORY_H_
