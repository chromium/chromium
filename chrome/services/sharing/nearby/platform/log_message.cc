// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/log_message.h"

#include <algorithm>

#include "base/strings/stringprintf.h"

namespace nearby {
namespace chrome {

api::LogMessage::Severity g_min_log_severity = api::LogMessage::Severity::kInfo;

logging::LogSeverity ConvertSeverity(api::LogMessage::Severity severity) {
  switch (severity) {
    case api::LogMessage::Severity::kVerbose:
      return logging::LOGGING_VERBOSE;
    case api::LogMessage::Severity::kInfo:
      return logging::LOGGING_INFO;
    case api::LogMessage::Severity::kWarning:
      return logging::LOGGING_WARNING;
    case api::LogMessage::Severity::kError:
      return logging::LOGGING_ERROR;
    case api::LogMessage::Severity::kFatal:
      return logging::LOGGING_FATAL;
  }
}

LogMessage::LogMessage(const char* file, int line, Severity severity)
    : log_message_(file, line, ConvertSeverity(severity)) {}

LogMessage::~LogMessage() = default;

void LogMessage::Print(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  log_message_.stream() << base::StringPrintV(format, ap);
  va_end(ap);
}

std::ostream& LogMessage::Stream() {
  return log_message_.stream();
}

}  // namespace chrome

namespace api {

// static
void LogMessage::SetMinLogSeverity(Severity severity) {
  chrome::g_min_log_severity = severity;
}

// static
bool LogMessage::ShouldCreateLogMessage(Severity severity) {
  return severity >= chrome::g_min_log_severity;
}

}  // namespace api
}  // namespace nearby
