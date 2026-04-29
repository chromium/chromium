// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/logger.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/omnibox/common/omnibox_features.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace omnibox {
namespace {

constexpr size_t kMaxRecentLogMessages = 700;

}  // namespace

Logger::LogMessageBuilder::LogMessageBuilder(const std::string& tag,
                                             const std::string& source_file,
                                             uint32_t source_line,
                                             Logger* logger)
    : tag_(tag),
      source_file_(source_file),
      source_line_(source_line),
      logger_(logger) {}

Logger::LogMessageBuilder::~LogMessageBuilder() {
  std::string message = base::StrCat(messages_);
  // Log the message before we move the strings into the logger.
  DVLOG(1) << source_file_ << "(" << source_line_ << ") " << message;

  // Move the builder's state into the logger to avoid unnecessary copies.
  // This is safe as the builder is about to be destroyed.
  logger_->OnLogMessageAdded(
      base::Time::Now(), std::move(tag_), std::move(source_file_), source_line_,
      std::move(message), std::move(proto_type_), std::move(proto_base64_));
}

Logger::LogMessageBuilder& Logger::LogMessageBuilder::operator<<(
    const char* message) {
  messages_.emplace_back(message);
  return *this;
}

Logger::LogMessageBuilder& Logger::LogMessageBuilder::operator<<(
    const std::string& message) {
  messages_.push_back(message);
  return *this;
}

Logger::LogMessageBuilder& Logger::LogMessageBuilder::operator<<(int message) {
  messages_.push_back(base::NumberToString(message));
  return *this;
}

Logger::LogMessageBuilder& Logger::LogMessageBuilder::operator<<(
    size_t message) {
  messages_.push_back(base::NumberToString(message));
  return *this;
}

Logger::LogMessageBuilder& Logger::LogMessageBuilder::operator<<(
    const GURL& message) {
  messages_.push_back(message.spec());
  return *this;
}

Logger::LogMessageBuilder& Logger::LogMessageBuilder::WithProto(
    const ::google::protobuf::MessageLite& proto,
    base::optional_ref<const std::string> type_name) {
  proto_type_ = type_name.has_value() ? std::string(*type_name)
                                      : std::string(proto.GetTypeName());
  // The API for opening Protoshop expects base64 encoded strings. We encode
  // the data here to simplify the flow, even though it takes more memory
  // than passing raw bytes and converting in JS.
  proto_base64_ = base::Base64Encode(proto.SerializeAsString());
  return *this;
}

Logger::LogMessage::LogMessage(base::Time event_time,
                               std::string tag,
                               std::string source_file,
                               uint32_t source_line,
                               std::string message,
                               std::optional<std::string> proto_type,
                               std::optional<std::string> proto_base64)
    : event_time(event_time),
      tag(std::move(tag)),
      source_file(std::move(source_file)),
      source_line(source_line),
      message(std::move(message)),
      proto_type(std::move(proto_type)),
      proto_base64(std::move(proto_base64)) {}

Logger::LogMessage::LogMessage(const LogMessage&) = default;
Logger::LogMessage::~LogMessage() = default;

// static
Logger* Logger::GetInstance() {
  static base::NoDestructor<Logger> instance;
  return instance.get();
}

Logger::Logger()
    : logging_enabled_(base::FeatureList::IsEnabled(kOmniboxDebugLogs)) {
  if (logging_enabled_) {
    recent_log_messages_.reserve(kMaxRecentLogMessages);
  }
}

Logger::~Logger() = default;

void Logger::AddObserver(Logger::Observer* observer) {
  observers_.AddObserver(observer);
  if (logging_enabled_) {
    for (const auto& message : recent_log_messages_) {
      for (Observer& obs : observers_) {
        obs.OnLogMessageAdded(message.event_time, message.tag,
                              message.source_file, message.source_line,
                              message.message, message.proto_type,
                              message.proto_base64);
      }
    }
  }
}

void Logger::RemoveObserver(Logger::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Logger::OnLogMessageAdded(base::Time event_time,
                               std::string tag,
                               std::string source_file,
                               uint32_t source_line,
                               std::string message,
                               std::optional<std::string> proto_type,
                               std::optional<std::string> proto_base64) {
  if (logging_enabled_) {
    recent_log_messages_.emplace_back(
        event_time, std::move(tag), std::move(source_file), source_line,
        std::move(message), std::move(proto_type), std::move(proto_base64));
    if (recent_log_messages_.size() > kMaxRecentLogMessages) {
      recent_log_messages_.pop_front();
    }
    // After moving the data into `recent_log_messages_`, we must use the newly
    // added element to notify observers, as the local parameters are now in
    // an indeterminate state.
    const LogMessage& log_message = recent_log_messages_.back();
    for (Observer& obs : observers_) {
      obs.OnLogMessageAdded(log_message.event_time, log_message.tag,
                            log_message.source_file, log_message.source_line,
                            log_message.message, log_message.proto_type,
                            log_message.proto_base64);
    }
  } else {
    // If logging is disabled, the data is not moved into the internal state,
    // so we can pass the parameters directly to the observers.
    for (Observer& obs : observers_) {
      obs.OnLogMessageAdded(event_time, tag, source_file, source_line, message,
                            proto_type, proto_base64);
    }
  }
}

bool Logger::ShouldEnableDebugLogs() const {
  return !observers_.empty() || logging_enabled_;
}

}  // namespace omnibox
