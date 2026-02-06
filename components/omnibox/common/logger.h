// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_LOGGER_H_
#define COMPONENTS_OMNIBOX_COMMON_LOGGER_H_

#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace omnibox {

#define OMNIBOX_LOG(tag)                                      \
  omnibox::Logger::LogMessageBuilder(tag, __FILE__, __LINE__, \
                                     omnibox::Logger::GetInstance())

// Interface to record the debug logs and show them on an internals page.
class Logger {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLogMessageAdded(base::Time event_time,
                                   const std::string& tag,
                                   const std::string& source_file,
                                   uint32_t source_line,
                                   const std::string& message) = 0;
  };
  static Logger* GetInstance();
  Logger();
  ~Logger();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void AddObserver(Logger::Observer* observer);
  void RemoveObserver(Logger::Observer* observer);
  void OnLogMessageAdded(base::Time event_time,
                         const std::string& tag,
                         const std::string& source_file,
                         uint32_t source_line,
                         const std::string& message);

  // Whether debug logs should allowed to be recorded.
  bool ShouldEnableDebugLogs() const;

  base::WeakPtr<Logger> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  // Class that builds the log message and used when debugging is enabled via
  // command-line switch or the internals page.
  class LogMessageBuilder {
   public:
    LogMessageBuilder(const std::string& tag,
                      const std::string& source_file,
                      uint32_t source_line,
                      Logger* Logger);
    ~LogMessageBuilder();

    LogMessageBuilder& operator<<(const char* message);
    LogMessageBuilder& operator<<(const std::string& message);

   private:
    const std::string tag_;
    const std::string source_file_;
    const uint32_t source_line_;
    std::vector<std::string> messages_;
    const raw_ptr<Logger> logger_;
  };

 private:
  struct LogMessage {
    LogMessage(base::Time event_time,
               const std::string& tag,
               const std::string& source_file,
               uint32_t source_line,
               const std::string& message);
    LogMessage(const LogMessage&);
    LogMessage& operator=(const LogMessage&) = delete;
    ~LogMessage();
    const base::Time event_time;
    const std::string tag;
    const std::string source_file;
    const uint32_t source_line;
    const std::string message;
  };

  // Contains the most recent log messages. Messages are queued up only when
  // |kDebugLoggingEnabled| command-line switch is specified. This allows the
  // messages at startup to be saved and shown in the internals page later.
  base::circular_deque<LogMessage> recent_log_messages_;

  base::ObserverList<Logger::Observer> observers_;

  bool logging_enabled_ = false;

  base::WeakPtrFactory<Logger> weak_ptr_factory_{this};
};

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_LOGGER_H_
