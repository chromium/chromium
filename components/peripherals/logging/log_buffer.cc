// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/peripherals/logging/log_buffer.h"

#include "base/no_destructor.h"

namespace {

// TODO(dpad): Decide on a reasonable max number of logs here.
// The maximum number of logs that can be stored in the buffer.
constexpr size_t kMaxBufferSize = 100;

}  // namespace

PeripheralsLogBuffer::LogMessage::LogMessage(const std::string& text,
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

PeripheralsLogBuffer::LogMessage::LogMessage(const LogMessage& message) =
    default;
PeripheralsLogBuffer::PeripheralsLogBuffer() = default;

PeripheralsLogBuffer::~PeripheralsLogBuffer() = default;

PeripheralsLogBuffer* PeripheralsLogBuffer::GetInstance() {
  static base::NoDestructor<PeripheralsLogBuffer> log_buffer;
  return log_buffer.get();
}

void PeripheralsLogBuffer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PeripheralsLogBuffer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PeripheralsLogBuffer::AddLogMessage(const LogMessage& log_message) {
  log_messages_.push_back(log_message);
  if (log_messages_.size() > MaxBufferSize()) {
    log_messages_.pop_front();
  }

  for (auto& observer : observers_) {
    observer.OnLogMessageAdded(log_message);
  }
}

void PeripheralsLogBuffer::Clear() {
  log_messages_.clear();

  for (auto& observer : observers_) {
    observer.OnPeripheralsLogBufferCleared();
  }
}

size_t PeripheralsLogBuffer::MaxBufferSize() const {
  return kMaxBufferSize;
}
