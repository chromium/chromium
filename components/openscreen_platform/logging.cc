// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/openscreen/src/platform/api/logging.h"

#include <cstring>
#include <sstream>
#include <string_view>

#include "base/debug/debugger.h"
#include "base/immediate_crash.h"
#include "base/logging.h"

namespace openscreen {

namespace {

::logging::LogSeverity MapLogLevel(LogLevel level) {
  switch (level) {
    case LogLevel::kVerbose:
      return ::logging::LOGGING_VERBOSE;
    case LogLevel::kInfo:
      return ::logging::LOGGING_INFO;
    case LogLevel::kWarning:
      return ::logging::LOGGING_WARNING;
    case LogLevel::kError:
      return ::logging::LOGGING_ERROR;
    case LogLevel::kFatal:
      return ::logging::LOGGING_FATAL;
  }
}

}  // namespace

bool IsLoggingOn(LogLevel level, std::string_view file) {
  if (level == LogLevel::kVerbose) {
    return ::logging::GetVlogLevelHelper(file.data(), file.size()) > 0;
  }
  return ::logging::ShouldCreateLogMessage(MapLogLevel(level));
}

void LogWithLevel(LogLevel level,
                  const char* file,
                  int line,
                  std::stringstream message) {
  ::logging::LogMessage(file, line, MapLogLevel(level)).stream()
      << message.rdbuf();
}

void Break() {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  base::ImmediateCrash();
#else
  // Chrome's base::debug::BreakDebugger is not properly annotated as
  // [[noreturn]], so we abort instead. This may need to be revisited
  // if we want MSVC support in the future.
  std::abort();
#endif
}

}  // namespace openscreen
