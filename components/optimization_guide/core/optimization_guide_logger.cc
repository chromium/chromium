// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_logger.h"

#include "components/optimization_guide/core/optimization_guide_switches.h"

namespace {

// TODO(rajendrant): Verify if all debug messages before browser startup are
// getting saved without being dropped, when some hints fetching and model
// downloading happens.
constexpr size_t kMaxRecentLogMessages = 100;

}  // namespace

OptimizationGuideLogger::LogMessage::LogMessage(base::Time event_time,
                                                const std::string& source_file,
                                                int source_line,
                                                const std::string& message)
    : event_time(event_time),
      source_file(source_file),
      source_line(source_line),
      message(message) {}

OptimizationGuideLogger::OptimizationGuideLogger() {
  if (optimization_guide::switches::IsDebugLogsEnabled())
    recent_log_messages_.reserve(kMaxRecentLogMessages);
}

OptimizationGuideLogger::~OptimizationGuideLogger() = default;

void OptimizationGuideLogger::AddObserver(
    OptimizationGuideLogger::Observer* observer) {
  observers_.AddObserver(observer);
  if (optimization_guide::switches::IsDebugLogsEnabled()) {
    for (const auto& message : recent_log_messages_) {
      for (Observer& obs : observers_) {
        obs.OnLogMessageAdded(message.event_time, message.source_file,
                              message.source_line, message.message);
      }
    }
  }
}

void OptimizationGuideLogger::RemoveObserver(
    OptimizationGuideLogger::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OptimizationGuideLogger::OnLogMessageAdded(base::Time event_time,
                                                const std::string& source_file,
                                                int source_line,
                                                const std::string& message) {
  if (optimization_guide::switches::IsDebugLogsEnabled()) {
    recent_log_messages_.emplace_back(event_time, source_file, source_line,
                                      message);
    if (recent_log_messages_.size() > kMaxRecentLogMessages)
      recent_log_messages_.pop_front();
  }
  for (Observer& obs : observers_)
    obs.OnLogMessageAdded(event_time, source_file, source_line, message);
}

bool OptimizationGuideLogger::ShouldEnableDebugLogs() const {
  return !observers_.empty() ||
         optimization_guide::switches::IsDebugLogsEnabled();
}
