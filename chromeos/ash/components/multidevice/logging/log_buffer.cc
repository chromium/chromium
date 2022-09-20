// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/logging/log_buffer.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace ash::multidevice {

namespace {

// The maximum number of logs that can be stored in the buffer.
const size_t kMaxBufferSize = 1000;

base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

}  // namespace

LogBuffer::LogMessage::LogMessage(const std::string& text,
                                  const base::Time& time,
                                  const std::string& file,
                                  const int line,
                                  logging::LogSeverity severity)
    : text(text), time(time), file(file), line(line), severity(severity) {}

LogBuffer::LogBuffer() {}

LogBuffer::~LogBuffer() {}

// static
LogBuffer* LogBuffer::GetInstance() {
  base::AutoLock guard(GetLock());

  static base::NoDestructor<LogBuffer> log_buffer;
  return log_buffer.get();
}

void LogBuffer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LogBuffer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void LogBuffer::AddLogMessage(const LogMessage& log_message) {
  base::AutoLock guard(GetLock());

  // Note: We may want to sort the messages by timestamp if there are cases
  // where logs are not added chronologically.
  log_messages_.push_back(log_message);
  if (log_messages_.size() > MaxBufferSize())
    log_messages_.pop_front();
  for (auto& observer : observers_)
    observer.OnLogMessageAdded(log_message);
}

void LogBuffer::Clear() {
  base::AutoLock guard(GetLock());

  log_messages_.clear();
  for (auto& observer : observers_)
    observer.OnLogBufferCleared();
}

size_t LogBuffer::MaxBufferSize() const {
  return kMaxBufferSize;
}

}  // namespace ash::multidevice
