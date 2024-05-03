// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/peripherals/logging/logging.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

PeripheralsScopedLogMessage::PeripheralsScopedLogMessage(
    const char* file,
    int line,
    logging::LogSeverity severity,
    Feature feature)
    : file_(file), feature_(feature), line_(line), severity_(severity) {}

std::string_view GetFeaturePrefix(Feature feature) {
  switch (feature) {
    case Feature::ACCEL:
      return "[ACCEL]";
    case Feature::IDS:
      return "[IDS]";
  }
}

PeripheralsScopedLogMessage::~PeripheralsScopedLogMessage() {
  // For now, only emit logs if they are warning or more severe OR if the flag
  // is enabled.
  const std::string string_from_stream =
      base::JoinString({GetFeaturePrefix(feature_), stream_.str()}, " ");
  if (ash::features::IsPeripheralsLoggingEnabled()) {
    // TODO(dpad): Utilize the logs in the buffer in feedback reports.
    PeripheralsLogBuffer::GetInstance()->AddLogMessage(
        PeripheralsLogBuffer::LogMessage(string_from_stream, feature_,
                                         base::Time::Now(), file_, line_,
                                         severity_));
  }

  // Don't emit VERBOSE-level logging to the standard logging system.
  if (severity_ <= logging::LOGGING_WARNING &&
      !ash::features::IsPeripheralsLoggingEnabled()) {
    return;
  }

  // The destructor of |log_message| also creates a log for the standard logging
  // system.
  logging::LogMessage log_message(file_, line_, severity_);
  log_message.stream() << string_from_stream;
}
