// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CROSS_DEVICE_LOGGING_LOGGING_H_
#define COMPONENTS_CROSS_DEVICE_LOGGING_LOGGING_H_

#include <sstream>
#include <string_view>

#include "base/logging.h"
#include "components/cross_device/logging/log_buffer.h"

// Use the CD_LOG() macro for all logging related to Cross Device Features so
// the debug page can reflect all logs related to this feature in the internal
// debug WebUI (chrome://nearby-internals).
#define CD_LOG(severity, feature)                                              \
  CrossDeviceScopedLogMessage(std::string_view(__FILE__, std::size(__FILE__)), \
                              __LINE__, logging::LOGGING_##severity, feature)  \
      .stream()

// An intermediate object used by the CD_LOG macro, wrapping a
// logging::LogMessage instance. When this object is destroyed, the message will
// be logged with the standard logging system and also added to Nearby Sharing
// specific log buffer.
class CrossDeviceScopedLogMessage {
 public:
  CrossDeviceScopedLogMessage(std::string_view file,
                              int line,
                              logging::LogSeverity severity,
                              Feature feature);
  CrossDeviceScopedLogMessage(const CrossDeviceScopedLogMessage&) = delete;
  CrossDeviceScopedLogMessage& operator=(const CrossDeviceScopedLogMessage&) =
      delete;
  ~CrossDeviceScopedLogMessage();

  std::ostream& stream() { return stream_; }

 private:
  const std::string_view file_;
  Feature feature_;
  int line_;
  logging::LogSeverity severity_;
  std::ostringstream stream_;
};

#endif  // COMPONENTS_CROSS_DEVICE_LOGGING_LOGGING_H_
