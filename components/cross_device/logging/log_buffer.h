// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CROSS_DEVICE_LOGGING_LOG_BUFFER_H_
#define COMPONENTS_CROSS_DEVICE_LOGGING_LOG_BUFFER_H_

#include <stddef.h>

#include <list>

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/time/time.h"

enum class Feature {
  NS = 0,
  NEARBY_INFRA = 1,
  FP = 2,
};

// Contains logs specific to Nearby Sharing. This buffer has a maximum size
// and will discard entries in FIFO order.
// Call CrossDeviceLogBuffer::GetInstance() to get the global
// CrossDeviceLogBuffer instance.
class CrossDeviceLogBuffer {
 public:
  // Represents a single log entry in the log buffer.
  struct LogMessage {
    const std::string text;
    Feature feature;
    base::Time time;
    const std::string file;
    int line;
    const logging::LogSeverity severity;

    LogMessage(const std::string& text,
               Feature feature,
               base::Time time,
               const std::string& file,
               int line,
               logging::LogSeverity severity);
    LogMessage(const LogMessage&);
  };

  class Observer {
   public:
    // Called when a new message is added to the log buffer.
    virtual void OnLogMessageAdded(const LogMessage& log_message) = 0;

    // Called when all messages in the log buffer are cleared.
    virtual void OnCrossDeviceLogBufferCleared() = 0;
  };

  CrossDeviceLogBuffer();
  CrossDeviceLogBuffer(const CrossDeviceLogBuffer&) = delete;
  CrossDeviceLogBuffer& operator=(const CrossDeviceLogBuffer&) = delete;
  ~CrossDeviceLogBuffer();

  // Returns the global instance.
  static CrossDeviceLogBuffer* GetInstance();

  // Adds and removes log buffer observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Adds a new log message to the buffer. If the number of log messages exceeds
  // the maximum, then the earliest added log will be removed.
  void AddLogMessage(const LogMessage& log_message);

  // Clears all logs in the buffer.
  void Clear();

  // Returns the maximum number of logs that can be stored.
  size_t MaxBufferSize() const;

  // Returns the list of logs in the buffer.
  const std::list<LogMessage>* logs() { return &log_messages_; }

 private:
  std::list<LogMessage> log_messages_;
  base::ObserverList<Observer>::Unchecked observers_;
};

#endif  // COMPONENTS_CROSS_DEVICE_LOGGING_LOG_BUFFER_H_
