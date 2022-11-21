// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_event_log/device_event_log.h"

#include <string>

#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log_impl.h"

namespace device_event_log {

namespace {

const size_t kDefaultMaxEntries = 4000;

const int kSlowMethodThresholdMs = 10;
// Loaded builders may perform badly, set this to a fairly high value to catch
// extreme cases.
const int kVerySlowMethodThresholdMs = 250;

DeviceEventLogImpl* g_device_event_log = nullptr;

}  // namespace

const LogLevel kDefaultLogLevel = LOG_LEVEL_EVENT;

void Initialize(size_t max_entries) {
  CHECK(!g_device_event_log);
  if (max_entries == 0)
    max_entries = kDefaultMaxEntries;
  g_device_event_log = new DeviceEventLogImpl(
      base::SingleThreadTaskRunner::GetCurrentDefault(), max_entries);
}

bool IsInitialized() {
  return !!g_device_event_log;
}

void Shutdown() {
  delete g_device_event_log;
  g_device_event_log = nullptr;
}

void AddEntry(const char* file,
              int line,
              LogType type,
              LogLevel level,
              const std::string& event) {
  if (g_device_event_log) {
    g_device_event_log->AddEntry(file, line, type, level, event);
  } else {
    DeviceEventLogImpl::SendToVLogOrErrorLog(file, line, type, level, event);
  }
}

void AddEntryWithDescription(const char* file,
                             int line,
                             LogType type,
                             LogLevel level,
                             const std::string& event,
                             const std::string& desc) {
  std::string event_with_desc = event;
  if (!desc.empty())
    event_with_desc += ": " + desc;
  AddEntry(file, line, type, level, event_with_desc);
}

std::string GetAsString(StringOrder order,
                        const std::string& format,
                        const std::string& types,
                        LogLevel max_level,
                        size_t max_events) {
  if (!g_device_event_log)
    return "DeviceEventLog not initialized.";
  return g_device_event_log->GetAsString(order, format, types, max_level,
                                         max_events);
}

void ClearAll() {
  if (g_device_event_log)
    g_device_event_log->ClearAll();
}

void Clear(const base::Time& begin, const base::Time& end) {
  if (g_device_event_log)
    g_device_event_log->Clear(begin, end);
}

int GetCountByLevelForTesting(LogLevel level) {
  return g_device_event_log->GetCountByLevelForTesting(level);
}

namespace internal {

DeviceEventLogInstance::DeviceEventLogInstance(const char* file,
                                               int line,
                                               device_event_log::LogType type,
                                               device_event_log::LogLevel level)
    : file_(file), line_(line), type_(type), level_(level) {
}

DeviceEventLogInstance::~DeviceEventLogInstance() {
  device_event_log::AddEntry(file_, line_, type_, level_, stream_.str());
}

DeviceEventSystemErrorLogInstance::DeviceEventSystemErrorLogInstance(
    const char* file,
    int line,
    device_event_log::LogType type,
    device_event_log::LogLevel level,
    logging::SystemErrorCode err)
    : err_(err), log_instance_(file, line, type, level) {
}

DeviceEventSystemErrorLogInstance::~DeviceEventSystemErrorLogInstance() {
  stream() << ": " << ::logging::SystemErrorCodeToString(err_);
}

ScopedDeviceLogIfSlow::ScopedDeviceLogIfSlow(LogType type,
                                             const char* file,
                                             const std::string& name)
    : file_(file), type_(type), name_(name) {
}

ScopedDeviceLogIfSlow::~ScopedDeviceLogIfSlow() {
  if (timer_.Elapsed().InMilliseconds() >= kSlowMethodThresholdMs) {
    LogLevel level(LOG_LEVEL_DEBUG);
    if (timer_.Elapsed().InMilliseconds() >= kVerySlowMethodThresholdMs)
      level = LOG_LEVEL_ERROR;
    DEVICE_LOG(type_, level) << "@@@ Slow method: " << file_ << ":" << name_
                             << ": " << timer_.Elapsed().InMilliseconds()
                             << "ms";
  }
}

}  // namespace internal

}  // namespace device_event_log
