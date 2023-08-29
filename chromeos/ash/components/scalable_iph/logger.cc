// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/logger.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"

namespace scalable_iph {

namespace {

// A practically impossible to reach limit of number of log messages. This is
// for receiving 100 log messages for a minute for 200 hours. If an average size
// of a single log message is 200bytes, this will be ~240MB.
constexpr int kLogSizeLimit = 100 * 200 * 60;

}  // namespace

// static
bool Logger::IsEnabled() {
  return ash::features::IsScalableIphDebugEnabled();
}

Logger::Logger() = default;
Logger::~Logger() = default;

void Logger::Log(const base::Location& location, std::string_view log_string) {
  if (!IsEnabled()) {
    return;
  }

  CHECK(logs_.size() < kLogSizeLimit)
      << "Log size reached to its internal limit.";

  const std::string& log = base::JoinString(
      {ToString(base::Time::Now()), ToString(location), log_string}, ": ");

  DLOG(WARNING) << log;

  logs_.push_back(std::move(log));
}

std::string Logger::GenerateLog() {
  CHECK(IsEnabled());

  return base::JoinString(logs_, "\n");
}

bool Logger::IsLogEmptyForTesting() {
  return logs_.empty();
}

LogMessage::LogMessage(Logger* logger, base::Location location)
    : logger_(logger), location_(location) {
  CHECK(logger_);
}

LogMessage::~LogMessage() {
  logger_->Log(location_, stream_.str());
}

}  // namespace scalable_iph
