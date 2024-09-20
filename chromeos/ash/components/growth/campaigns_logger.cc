// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/growth/campaigns_logger.h"

#include <sstream>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/syslog_logging.h"
#include "base/time/time.h"

namespace growth {

namespace {

CampaignsLogger* g_instance = nullptr;

constexpr int kMaxLogs = 1000;

std::string ToString(LogLevel level) {
  switch (level) {
    case LogLevel::kERROR:
      return "ERROR";
    case LogLevel::kSYSLOG:
      return "SYSLOG";
    case LogLevel::kVLOG:
      return "VLOG";
    case LogLevel::kDEBUG:
      return "DEBUG";
  }
  NOTREACHED();
}

void SendToLog(LogLevel level, const std::string& log) {
  switch (level) {
    case LogLevel::kERROR:
      LOG(ERROR) << log;
      break;
    case LogLevel::kSYSLOG:
      SYSLOG(INFO) << log;
      break;
    case LogLevel::kVLOG:
      VLOG(2) << log;
      break;
    default:
      // Do nothing.
      break;
  }
}

}  // namespace

// static
CampaignsLogger* CampaignsLogger::Get() {
  CHECK(g_instance);
  return g_instance;
}

CampaignsLogger::CampaignsLogger() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

CampaignsLogger::~CampaignsLogger() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

std::vector<std::string> CampaignsLogger::GetLogs() {
  return std::vector<std::string>(logs_.begin(), logs_.end());
}

bool CampaignsLogger::HasLogForTesting() {
  return !logs_.empty();
}

void CampaignsLogger::Log(LogLevel level,
                          const base::Location& location,
                          std::string_view log_string) {
  if (logs_.size() >= kMaxLogs) {
    logs_.pop_front();
  }

  std::string log = base::JoinString({ToString(location), log_string}, ": ");
  SendToLog(level, log);

  if (!ash::features::IsGrowthInternalsEnabled()) {
    return;
  }

  log = base::JoinString({ToString(base::Time::Now()), ToString(level), log},
                         ": ");
  logs_.push_back(std::move(log));
}

LogMessage::LogMessage(LogLevel level, base::Location location)
    : level_(level), location_(location) {}

LogMessage::~LogMessage() {
  CampaignsLogger::Get()->Log(level_, location_, stream_.str());
}

}  // namespace growth
