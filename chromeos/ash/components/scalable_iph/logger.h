// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_LOGGER_H_

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"

// Example usage:
// SCALABLE_IPH_LOG(logger()) << "Log message";
#define SCALABLE_IPH_LOG(logger)                                    \
  LAZY_STREAM(scalable_iph::LogMessage(logger, FROM_HERE).stream(), \
              scalable_iph::Logger::IsEnabled())

namespace scalable_iph {

// A logger for Scalable Iph.
//
// This is an opt-in debug feature. You have to enable
// `ash::features::kScalableIphDebug` to use/enable this feature. This class
// stores logs in memory. Note that logs are also sent to DLOG(WARNING). It
// means that logs can be stored on a disk or other places. You can access logs
// from chrome-untrusted://scalable-iph-debug/logs.txt.
class Logger {
 public:
  static bool IsEnabled();

  Logger();
  ~Logger();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  // Use SCALABLE_IPH_LOG macro instead of directly calling this method.
  void Log(const base::Location& location, std::string_view log_string);

  std::string GenerateLog();
  bool IsLogEmptyForTesting();

 private:
  std::vector<std::string> logs_;
};

class LogMessage {
 public:
  LogMessage(Logger* logger, base::Location location);
  ~LogMessage();

  std::ostream& stream() { return stream_; }

 private:
  const raw_ptr<Logger> logger_;
  const base::Location location_;

  std::ostringstream stream_;
};

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_LOGGER_H_
