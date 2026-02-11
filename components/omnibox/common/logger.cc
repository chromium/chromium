// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/logger.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/omnibox/common/omnibox_features.h"

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
  logger_->OnLogMessageAdded(base::Time::Now(), tag_, source_file_,
                             source_line_, message);
  DVLOG(1) << source_file_ << "(" << source_line_ << ") " << message;
}

Logger::LogMessageBuilder& Logger::LogMessageBuilder::operator<<(
    const char* message) {
  messages_.push_back(message);
  return *this;
}

Logger::LogMessageBuilder& Logger::LogMessageBuilder::operator<<(
    const std::string& message) {
  messages_.push_back(message);
  return *this;
}

Logger::LogMessage::LogMessage(base::Time event_time,
                               const std::string& tag,
                               const std::string& source_file,
                               uint32_t source_line,
                               const std::string& message)
    : event_time(event_time),
      tag(tag),
      source_file(source_file),
      source_line(source_line),
      message(message) {}

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
                              message.message);
      }
    }
  }
}

void Logger::RemoveObserver(Logger::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Logger::OnLogMessageAdded(base::Time event_time,
                               const std::string& tag,
                               const std::string& source_file,
                               uint32_t source_line,
                               const std::string& message) {
  if (logging_enabled_) {
    recent_log_messages_.emplace_back(event_time, tag, source_file, source_line,
                                      message);
    if (recent_log_messages_.size() > kMaxRecentLogMessages) {
      recent_log_messages_.pop_front();
    }
  }
  for (Observer& obs : observers_) {
    obs.OnLogMessageAdded(event_time, tag, source_file, source_line, message);
  }
}

bool Logger::ShouldEnableDebugLogs() const {
  return !observers_.empty() || logging_enabled_;
}

}  // namespace omnibox
