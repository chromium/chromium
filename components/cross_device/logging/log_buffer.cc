// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cross_device/logging/log_buffer.h"
#include "base/no_destructor.h"

namespace {

// The maximum number of logs that can be stored in the buffer.
constexpr size_t kMaxBufferSize = 10000;

}  // namespace

CrossDeviceLogBuffer::LogMessage::LogMessage(const std::string& text,
                                             Feature feature,
                                             base::Time time,
                                             const std::string& file,
                                             int line,
                                             logging::LogSeverity severity)
    : text(text),
      feature(feature),
      time(time),
      file(file),
      line(line),
      severity(severity) {}

CrossDeviceLogBuffer::LogMessage::LogMessage(const LogMessage& message) =
    default;
CrossDeviceLogBuffer::CrossDeviceLogBuffer() = default;

CrossDeviceLogBuffer::~CrossDeviceLogBuffer() = default;

CrossDeviceLogBuffer* CrossDeviceLogBuffer::GetInstance() {
  static base::NoDestructor<CrossDeviceLogBuffer> log_buffer;
  return log_buffer.get();
}

void CrossDeviceLogBuffer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CrossDeviceLogBuffer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CrossDeviceLogBuffer::AddLogMessage(const LogMessage& log_message) {
  log_messages_.push_back(log_message);
  if (log_messages_.size() > MaxBufferSize()) {
    log_messages_.pop_front();
  }

  for (auto& observer : observers_) {
    observer.OnLogMessageAdded(log_message);
  }
}

void CrossDeviceLogBuffer::Clear() {
  log_messages_.clear();

  for (auto& observer : observers_) {
    observer.OnCrossDeviceLogBufferCleared();
  }
}

size_t CrossDeviceLogBuffer::MaxBufferSize() const {
  return kMaxBufferSize;
}
