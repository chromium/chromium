// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/logger_impl.h"

#include <string_view>

#include "base/i18n/time_formatting.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "components/media_router/browser/log_util.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/logger.mojom-shared.h"
#include "url/gurl.h"

namespace media_router {

namespace {

constexpr size_t kEntriesCapacity = 1000;

constexpr size_t kComponentMaxLength = 64;
constexpr size_t kMessageMaxLength = 1024;
constexpr size_t kSourceMaxLength = 64;

const char* AsString(LoggerImpl::Severity severity) {
  switch (severity) {
    case LoggerImpl::Severity::kInfo:
      return "Info";
    case LoggerImpl::Severity::kWarning:
      return "Warning";
    case LoggerImpl::Severity::kError:
      return "Error";
  }
}

const char* AsString(mojom::LogCategory category) {
  switch (category) {
    case mojom::LogCategory::kDiscovery:
      return "Discovery";
    case mojom::LogCategory::kRoute:
      return "Route";
    case mojom::LogCategory::kMirroring:
      return "Mirroring";
    case mojom::LogCategory::kUi:
      return "UI";
  }
}

std::string_view TruncateComponent(std::string_view component) {
  return component.substr(0, kComponentMaxLength);
}

std::string_view TruncateMessage(std::string_view message) {
  return message.substr(0, kMessageMaxLength);
}

}  // namespace

LoggerImpl::LoggerImpl() : capacity_(kEntriesCapacity) {}

LoggerImpl::~LoggerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LoggerImpl::LogInfo(mojom::LogCategory category,
                         const std::string& component,
                         const std::string& message,
                         const std::string& sink_id,
                         const std::string& media_source,
                         const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Log(Severity::kInfo, category, base::Time::Now(), component, message, sink_id,
      media_source, session_id);
}

void LoggerImpl::LogWarning(mojom::LogCategory category,
                            const std::string& component,
                            const std::string& message,
                            const std::string& sink_id,
                            const std::string& media_source,
                            const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Log(Severity::kWarning, category, base::Time::Now(), component, message,
      sink_id, media_source, session_id);
}

void LoggerImpl::LogError(mojom::LogCategory category,
                          const std::string& component,
                          const std::string& message,
                          const std::string& sink_id,
                          const std::string& media_source,
                          const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Log(Severity::kError, category, base::Time::Now(), component, message,
      sink_id, media_source, session_id);
}

void LoggerImpl::BindReceiver(mojo::PendingReceiver<mojom::Logger> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void LoggerImpl::Log(Severity severity,
                     mojom::LogCategory category,
                     base::Time time,
                     const std::string& component,
                     const std::string& message,
                     const std::string& sink_id,
                     const std::string& media_source,
                     const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  entries_.emplace_back(
      severity, category, time, TruncateComponent(component),
      TruncateMessage(message), log_util::TruncateId(sink_id),
      MediaSource(media_source).TruncateForLogging(kSourceMaxLength),
      log_util::TruncateId(session_id));
  if (entries_.size() > capacity_)
    entries_.pop_front();
}

std::string LoggerImpl::GetLogsAsJson() const {
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.set_pretty_print(true);
  if (!serializer.Serialize(GetLogsAsValue())) {
    DVLOG(1) << "Failed to serialize log to JSON.";
    return "";
  }
  return json;
}

base::Value LoggerImpl::GetLogsAsValue() const {
  base::Value::List entries_val;
  for (const auto& entry : entries_)
    entries_val.Append(AsValue(entry));
  return base::Value(std::move(entries_val));
}

LoggerImpl::Entry::Entry(Severity severity,
                         mojom::LogCategory category,
                         base::Time time,
                         std::string_view component,
                         std::string_view message,
                         std::string_view sink_id,
                         std::string media_source,
                         std::string_view session_id)
    : severity(severity),
      category(category),
      time(time),
      component(component),
      message(message),
      sink_id(sink_id),
      media_source(std::move(media_source)),
      session_id(session_id) {}

LoggerImpl::Entry::Entry(Entry&& other)
    : severity(other.severity),
      category(other.category),
      time(other.time),
      component(std::move(other.component)),
      message(std::move(other.message)),
      sink_id(std::move(other.sink_id)),
      media_source(std::move(other.media_source)),
      session_id(std::move(other.session_id)) {}

LoggerImpl::Entry::~Entry() = default;

// static
base::Value::Dict LoggerImpl::AsValue(const LoggerImpl::Entry& entry) {
  base::Value::Dict entry_val;
  entry_val.Set("severity", base::Value(AsString(entry.severity)));
  entry_val.Set("category", base::Value(AsString(entry.category)));
  entry_val.Set(
      "time",
      base::Value(base::TimeFormatTimeOfDayWithMilliseconds(entry.time)));
  entry_val.Set("component", base::Value(entry.component));
  entry_val.Set("message", base::Value(entry.message));
  entry_val.Set("sinkId", base::Value(entry.sink_id));
  entry_val.Set("mediaSource", base::Value(entry.media_source));
  entry_val.Set("sessionId", base::Value(entry.session_id));
  return entry_val;
}

}  // namespace media_router
