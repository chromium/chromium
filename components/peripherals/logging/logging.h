// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERIPHERALS_LOGGING_LOGGING_H_
#define COMPONENTS_PERIPHERALS_LOGGING_LOGGING_H_

#include <sstream>

#include "base/logging.h"
#include "components/peripherals/logging/log_buffer.h"

// Use the PR_LOG() macro for all logging related to Peripherals Features so
// the debug page can reflect all logs related to this feature in the internal
// debug WebUI (chrome://nearby-internals).
#define PR_LOG(severity, feature)                                              \
  PeripheralsScopedLogMessage(__FILE__, __LINE__, logging::LOGGING_##severity, \
                              feature)                                         \
      .stream()

// An intermediate object used by the PR_LOG macro, wrapping a
// logging::LogMessage instance. When this object is destroyed, the message will
// be logged with the standard logging system and also added to Peripherals
// specific log buffer.
class PeripheralsScopedLogMessage {
 public:
  PeripheralsScopedLogMessage(const char* file,
                              int line,
                              logging::LogSeverity severity,
                              Feature feature);
  PeripheralsScopedLogMessage(const PeripheralsScopedLogMessage&) = delete;
  PeripheralsScopedLogMessage& operator=(const PeripheralsScopedLogMessage&) =
      delete;
  ~PeripheralsScopedLogMessage();

  std::ostream& stream() { return stream_; }

 private:
  const char* file_;
  Feature feature_;
  int line_;
  logging::LogSeverity severity_;
  std::ostringstream stream_;
};

#endif  // COMPONENTS_PERIPHERALS_LOGGING_LOGGING_H_
