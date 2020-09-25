// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/logger_impl.h"

#include "base/i18n/time_formatting.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/logger.mojom-shared.h"
#include "url/gurl.h"

namespace media_router {

namespace {

constexpr size_t kEntriesCapacity = 1000;

constexpr size_t kComponentMaxLength = 64;
constexpr size_t kMessageMaxLength = 256;
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

base::StringPiece TruncateComponent(base::StringPiece component) {
  return component.substr(0, kComponentMaxLength);
}

base::StringPiece TruncateMessage(base::StringPiece message) {
  return message.substr(0, kMessageMaxLength);
}

// Gets the last four characters of an ID string.
base::StringPiece TruncateId(base::StringPiece id) {
  if (id.size() <= 4)
    return id;
  return id.substr(id.size() - 4);
}

}  // namespace

LoggerImpl::LoggerImpl() : capacity_(kEntriesCapacity) {}
LoggerImpl::~LoggerImpl() = default;

void LoggerImpl::LogInfo(mojom::LogCategory category,
                         const std::string& component,
                         const std::string& message,
                         const std::string& sink_id,
                         const std::string& media_source,
                         const std::string& session_id) {
  Log(Severity::kInfo, category, base::Time::Now(), component, message, sink_id,
      media_source, session_id);
}

void LoggerImpl::LogWarning(mojom::LogCategory category,
                            const std::string& component,
                            const std::string& message,
                            const std::string& sink_id,
                            const std::string& media_source,
                            const std::string& session_id) {
  Log(Severity::kWarning, category, base::Time::Now(), component, message,
      sink_id, media_source, session_id);
}

void LoggerImpl::LogError(mojom::LogCategory category,
                          const std::string& component,
                          const std::string& message,
                          const std::string& sink_id,
                          const std::string& media_source,
                          const std::string& session_id) {
  Log(Severity::kError, category, base::Time::Now(), component, message,
      sink_id, media_source, session_id);
}

void LoggerImpl::Bind(mojo::PendingReceiver<mojom::Logger> receiver) {
  receivers_.Add(this, std::move(receiver));
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
  base::Value entries_val(base::Value::Type::LIST);
  for (const auto& entry : entries_)
    entries_val.Append(AsValue(entry));
  return entries_val;
}

LoggerImpl::Entry::Entry(Severity severity,
                         mojom::LogCategory category,
                         base::Time time,
                         base::StringPiece component,
                         base::StringPiece message,
                         base::StringPiece sink_id,
                         std::string media_source,
                         base::StringPiece session_id)
    : severity(severity),
      category(category),
      time(time),
      component(component.as_string()),
      message(message.as_string()),
      sink_id(sink_id.as_string()),
      media_source(std::move(media_source)),
      session_id(session_id.as_string()) {}

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

void LoggerImpl::Log(Severity severity,
                     mojom::LogCategory category,
                     base::Time time,
                     const std::string& component,
                     const std::string& message,
                     const std::string& sink_id,
                     const std::string& media_source,
                     const std::string& session_id) {
  entries_.emplace_back(
      severity, category, time, TruncateComponent(component),
      TruncateMessage(message), TruncateId(sink_id),
      MediaSource(media_source).TruncateForLogging(kSourceMaxLength),
      TruncateId(session_id));
  if (entries_.size() > capacity_)
    entries_.pop_front();
}

// static
base::Value LoggerImpl::AsValue(const LoggerImpl::Entry& entry) {
  base::Value entry_val(base::Value::Type::DICTIONARY);
  entry_val.SetKey("severity", base::Value(AsString(entry.severity)));
  entry_val.SetKey("category", base::Value(AsString(entry.category)));
  entry_val.SetKey(
      "time",
      base::Value(base::TimeFormatTimeOfDayWithMilliseconds(entry.time)));
  entry_val.SetKey("component", base::Value(entry.component));
  entry_val.SetKey("message", base::Value(entry.message));
  entry_val.SetKey("sinkId", base::Value(entry.sink_id));
  entry_val.SetKey("mediaSource", base::Value(entry.media_source));
  entry_val.SetKey("sessionId", base::Value(entry.session_id));
  return entry_val;
}

}  // namespace media_router
