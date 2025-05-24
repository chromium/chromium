// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cross_device/logging/logging.h"
#include "base/command_line.h"

CrossDeviceScopedLogMessage::CrossDeviceScopedLogMessage(
    std::string_view file,
    int line,
    logging::LogSeverity severity,
    Feature feature)
    : file_(file), feature_(feature), line_(line), severity_(severity) {}

CrossDeviceScopedLogMessage::~CrossDeviceScopedLogMessage() {
  const std::string string_from_stream = stream_.str();
  CrossDeviceLogBuffer::GetInstance()->AddLogMessage(
      CrossDeviceLogBuffer::LogMessage(string_from_stream, feature_,
                                       base::Time::Now(), file_.data(), line_,
                                       severity_));

  // Don't emit VERBOSE-level logging to the standard logging system.
  if (severity_ <= logging::LOGGING_VERBOSE &&
      logging::GetVlogLevelHelper(file_.data(), file_.size()) <= 0) {
    return;
  }

  // The destructor of |log_message| also creates a log for the standard logging
  // system.
  logging::LogMessage log_message(file_.data(), line_, severity_);
  log_message.stream() << string_from_stream;
}
