// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_EVENT_LOGGER_H_
#define COMPONENTS_DRIVE_EVENT_LOGGER_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace drive {

// The default history size used by EventLogger.
const int kDefaultHistorySize = 1000;

// EventLogger is used to collect and expose text messages for diagnosing
// behaviors of Google APIs stuff. For instance, the collected messages are
// exposed to chrome:drive-internals.
class EventLogger {
 public:
  // Represents a single event log.
  struct Event {
    Event(int id, logging::LogSeverity severity, const std::string& what);
    int id;  // Monotonically increasing ID starting from 0.
    logging::LogSeverity severity;  // Severity of the event.
    base::Time when;  // When the event occurred.
    std::string what;  // What happened.
  };

  // Creates an event logger that keeps the latest kDefaultHistorySize events.
  EventLogger();

  EventLogger(const EventLogger&) = delete;
  EventLogger& operator=(const EventLogger&) = delete;

  ~EventLogger();

  // Logs a message and its severity.
  // Can be called from any thread as long as the object is alive.
  void LogRawString(logging::LogSeverity severity, const std::string& what);

  // Logs a message with formatting.
  // Can be called from any thread as long as the object is alive.
  PRINTF_FORMAT(3, 4)
  void Log(logging::LogSeverity severity, const char* format, ...);

  // Sets the history size. The existing history is cleared.
  // Can be called from any thread as long as the object is alive.
  void SetHistorySize(size_t history_size);

  // Gets the list of latest events (the oldest event comes first).
  // Can be called from any thread as long as the object is alive.
  std::vector<Event> GetHistory();

 private:
  base::circular_deque<Event> history_;  // guarded by lock_.
  size_t history_size_;  // guarded by lock_.
  int next_event_id_;  // guarded by lock_.
  base::Lock lock_;
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_EVENT_LOGGER_H_
