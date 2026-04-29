// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_LOGGER_H_
#define COMPONENTS_OMNIBOX_COMMON_LOGGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "url/gurl.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace omnibox {

#define OMNIBOX_LOG(tag)                                      \
  omnibox::Logger::LogMessageBuilder(tag, __FILE__, __LINE__, \
                                     omnibox::Logger::GetInstance())

#define OMNIBOX_LOG_WITH_PROTO(tag, proto, ...)                      \
  omnibox::Logger::LogMessageBuilder(tag, __FILE__, __LINE__,        \
                                     omnibox::Logger::GetInstance()) \
      .WithProto(proto, ##__VA_ARGS__)

// Interface to record the debug logs and show them on an internals page.
class Logger {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLogMessageAdded(
        base::Time event_time,
        const std::string& tag,
        const std::string& source_file,
        uint32_t source_line,
        const std::string& message,
        const std::optional<std::string>& proto_type,
        const std::optional<std::string>& proto_base64) = 0;
  };
  static Logger* GetInstance();
  Logger();
  ~Logger();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void AddObserver(Logger::Observer* observer);
  void RemoveObserver(Logger::Observer* observer);
  void OnLogMessageAdded(base::Time event_time,
                         std::string tag,
                         std::string source_file,
                         uint32_t source_line,
                         std::string message,
                         std::optional<std::string> proto_type,
                         std::optional<std::string> proto_base64);

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
                      Logger* logger);
    ~LogMessageBuilder();

    LogMessageBuilder& operator<<(const char* message);
    LogMessageBuilder& operator<<(const std::string& message);
    LogMessageBuilder& operator<<(int message);
    LogMessageBuilder& operator<<(size_t message);
    LogMessageBuilder& operator<<(const GURL& message);

    // Some protos have a different package name internally than what we roll
    // into the Chromium repository. The optional `type_name` allows overriding
    // the default type name obtained from the proto message itself.
    LogMessageBuilder& WithProto(
        const ::google::protobuf::MessageLite& proto,
        base::optional_ref<const std::string> type_name = std::nullopt);

   private:
    std::string tag_;
    std::string source_file_;
    const uint32_t source_line_;
    std::vector<std::string> messages_;
    std::optional<std::string> proto_type_;
    std::optional<std::string> proto_base64_;
    const raw_ptr<Logger> logger_;
  };

 private:
  struct LogMessage {
    LogMessage(base::Time event_time,
               std::string tag,
               std::string source_file,
               uint32_t source_line,
               std::string message,
               std::optional<std::string> proto_type,
               std::optional<std::string> proto_base64);
    LogMessage(const LogMessage&);
    LogMessage& operator=(const LogMessage&) = delete;
    ~LogMessage();
    const base::Time event_time;
    std::string tag;
    std::string source_file;
    const uint32_t source_line;
    std::string message;
    std::optional<std::string> proto_type;
    std::optional<std::string> proto_base64;
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
