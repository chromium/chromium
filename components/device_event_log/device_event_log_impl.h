// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_EVENT_LOG_DEVICE_EVENT_LOG_IMPL_H_
#define COMPONENTS_DEVICE_EVENT_LOG_DEVICE_EVENT_LOG_IMPL_H_

#include <stddef.h>

#include <list>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"

namespace device_event_log {

// Implementation class for DEVICE_LOG.
class DEVICE_EVENT_LOG_EXPORT DeviceEventLogImpl {
 public:
  struct LogEntry {
    LogEntry(const char* filedesc,
             int file_line,
             LogType log_type,
             LogLevel log_level,
             const std::string& event);
    LogEntry(const char* filedesc,
             int file_line,
             LogType log_type,
             LogLevel log_level,
             const std::string& event,
             base::Time time_for_testing);
    LogEntry(const LogEntry& other);

    std::string file;
    int file_line;
    LogType log_type;
    LogLevel log_level;
    std::string event;
    base::Time time;
    int count;
  };

  explicit DeviceEventLogImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      size_t max_entries);
  ~DeviceEventLogImpl();

  // Implements device_event_log::AddEntry.
  void AddEntry(const char* file,
                int file_line,
                LogType type,
                LogLevel level,
                const std::string& event);

  void AddEntryWithTimestampForTesting(const char* file,
                                       int file_line,
                                       LogType log_type,
                                       LogLevel log_level,
                                       const std::string& event,
                                       base::Time time);

  // Implements device_event_log::GetAsString.
  std::string GetAsString(StringOrder order,
                          const std::string& format,
                          const std::string& types,
                          LogLevel max_level,
                          size_t max_events);

  // Implements device_event_log::Clear.
  void ClearAll();
  void Clear(const base::Time& begin, const base::Time& end);

  // Called from device_event_log::AddEntry if the global instance has not been
  // created (or has already been destroyed). Logs to LOG(ERROR) or VLOG(1).
  static void SendToVLogOrErrorLog(const char* file,
                                   int file_line,
                                   LogType type,
                                   LogLevel log_level,
                                   const std::string& event);

 private:
  friend class DeviceEventLogTest;

  typedef std::list<LogEntry> LogEntryList;

  void AddLogEntry(const LogEntry& entry);
  void RemoveEntry();

  // For testing
  size_t max_entries() const { return max_entries_; }
  void set_max_entries_for_test(size_t entries) { max_entries_ = entries; }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  size_t max_entries_;
  LogEntryList entries_;
  base::WeakPtrFactory<DeviceEventLogImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceEventLogImpl);
};

}  // namespace device_event_log

#endif  // COMPONENTS_DEVICE_EVENT_LOG_DEVICE_EVENT_LOG_IMPL_H_
