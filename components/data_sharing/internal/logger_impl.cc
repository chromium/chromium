// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/logger_impl.h"

#include "base/command_line.h"
#include "components/data_sharing/public/switches.h"

namespace data_sharing {

LoggerImpl::LoggerImpl()
    : always_log_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          data_sharing::kDataSharingDebugLoggingEnabled)) {}

LoggerImpl::~LoggerImpl() = default;

void LoggerImpl::AddObserver(Observer* observer) {
  if (observers_.HasObserver(observer)) {
    return;
  }

  observers_.AddObserver(observer);

  for (const auto& entry : logs_) {
    observer->OnNewLog(entry);
  }
}

void LoggerImpl::RemoveObserver(Observer* observer) {
  if (!observers_.HasObserver(observer)) {
    return;
  }

  observers_.RemoveObserver(observer);

  if (!ShouldEnableDebugLogs()) {
    logs_.clear();
  }
}

bool LoggerImpl::ShouldEnableDebugLogs() {
  return always_log_ || !observers_.empty();
}

void LoggerImpl::Log(base::Time event_time,
                     logger_common::mojom::LogSource log_source,
                     const std::string& source_file,
                     int source_line,
                     const std::string& message) {
  VLOG(1) << log_source << ": " << message;

  if (!ShouldEnableDebugLogs()) {
    return;
  }

  Entry entry(event_time, log_source, source_file, source_line, message);

  logs_.push_back(entry);

  for (auto& observer : observers_) {
    observer.OnNewLog(entry);
  }
}

}  // namespace data_sharing
