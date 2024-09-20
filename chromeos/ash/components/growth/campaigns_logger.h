// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_LOGGER_H_

#include <deque>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"

// Examples:
//  CAMPAIGNS_LOG(ERROR) <<  "Error message";
//  CAMPAIGNS_LOG(DEBUG) <<  "Debug message";
#define CAMPAIGNS_LOG(level) \
  growth::LogMessage(growth::LogLevel::k##level, FROM_HERE).stream()

namespace growth {

class LogMessage;

// Used to specify the detail level for logging.
enum LogLevel { kERROR = 0, kSYSLOG = 1, kVLOG = 2, kDEBUG = 3 };

// A logger for growth campaigns.
// This logger stores logs in memory and send to LOG/SYSLOG/VLOG as needed.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH) CampaignsLogger {
 public:
  static CampaignsLogger* Get();

  CampaignsLogger();
  CampaignsLogger(const CampaignsLogger&) = delete;
  CampaignsLogger& operator=(const CampaignsLogger&) = delete;
  ~CampaignsLogger();

  std::vector<std::string> GetLogs();

  bool HasLogForTesting();

 private:
  friend LogMessage;

  void Log(LogLevel level,
           const base::Location& location,
           std::string_view log_string);

  std::deque<std::string> logs_;
};

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GROWTH) LogMessage {
 public:
  LogMessage(LogLevel level, base::Location location);
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  ~LogMessage();

  std::ostream& stream() { return stream_; }

 private:
  LogLevel level_;
  const base::Location location_;
  std::ostringstream stream_;
};

}  // namespace growth

#endif  // CHROMEOS_ASH_COMPONENTS_GROWTH_CAMPAIGNS_LOGGER_H_
