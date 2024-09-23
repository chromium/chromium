// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service_observer.h"

#include <string_view>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/metrics/metrics_logs_event_manager.h"

namespace metrics {
namespace {

MetricsServiceObserver::Log::Event CreateEventStruct(
    MetricsLogsEventManager::LogEvent event,
    std::string_view message) {
  MetricsServiceObserver::Log::Event event_struct;
  event_struct.event = event;
  event_struct.timestampMs =
      base::Time::Now().InMillisecondsFSinceUnixEpochIgnoringNull();
  if (!message.empty()) {
    event_struct.message = std::string(message);
  }
  return event_struct;
}

std::string LogTypeToString(MetricsLog::LogType log_type) {
  switch (log_type) {
    case MetricsLog::LogType::INDEPENDENT_LOG:
      return "Independent";
    case MetricsLog::LogType::INITIAL_STABILITY_LOG:
      return "Stability";
    case MetricsLog::LogType::ONGOING_LOG:
      return "Ongoing";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string EventToString(MetricsLogsEventManager::LogEvent event) {
  switch (event) {
    case MetricsLogsEventManager::LogEvent::kLogStaged:
      return "Staged";
    case MetricsLogsEventManager::LogEvent::kLogDiscarded:
      return "Discarded";
    case MetricsLogsEventManager::LogEvent::kLogTrimmed:
      return "Trimmed";
    case MetricsLogsEventManager::LogEvent::kLogUploading:
      return "Uploading";
    case MetricsLogsEventManager::LogEvent::kLogUploaded:
      return "Uploaded";
    case MetricsLogsEventManager::LogEvent::kLogCreated:
      return "Created";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string CreateReasonToString(
    metrics::MetricsLogsEventManager::CreateReason reason) {
  switch (reason) {
    case MetricsLogsEventManager::CreateReason::kUnknown:
      return std::string();
    case MetricsLogsEventManager::CreateReason::kPeriodic:
      return "Reason: Periodic log creation";
    case MetricsLogsEventManager::CreateReason::kServiceShutdown:
      return "Reason: Shutting down";
    case MetricsLogsEventManager::CreateReason::kLoadFromPreviousSession:
      return "Reason: Loaded from previous session";
    case MetricsLogsEventManager::CreateReason::kBackgrounded:
      return "Reason: Browser backgrounded";
    case MetricsLogsEventManager::CreateReason::kForegrounded:
      return "Reason: Browser foregrounded";
    case MetricsLogsEventManager::CreateReason::kAlternateOngoingLogStoreSet:
      return "Reason: Alternate ongoing log store set";
    case MetricsLogsEventManager::CreateReason::kAlternateOngoingLogStoreUnset:
      return "Reason: Alternate ongoing log store unset";
    case MetricsLogsEventManager::CreateReason::kStability:
      return "Reason: Stability metrics from previous session";
    case MetricsLogsEventManager::CreateReason::kIndependent:
      // TODO(crbug.com/40238818): Give more insight here (e.g. "independent log
      // generated from pma file").
      return "Reason: Independent log";
  }
}

}  // namespace

MetricsServiceObserver::MetricsServiceObserver(MetricsServiceType service_type)
    : service_type_(service_type) {}
MetricsServiceObserver::~MetricsServiceObserver() = default;
MetricsServiceObserver::Log::Log() = default;
MetricsServiceObserver::Log::Log(const Log&) = default;
MetricsServiceObserver::Log& MetricsServiceObserver::Log::operator=(
    const Log&) = default;
MetricsServiceObserver::Log::~Log() = default;
MetricsServiceObserver::Log::Event::Event() = default;
MetricsServiceObserver::Log::Event::Event(const Event&) = default;
MetricsServiceObserver::Log::Event&
MetricsServiceObserver::Log::Event::operator=(const Event&) = default;
MetricsServiceObserver::Log::Event::~Event() = default;

void MetricsServiceObserver::OnLogCreated(
    std::string_view log_hash,
    std::string_view log_data,
    std::string_view log_timestamp,
    metrics::MetricsLogsEventManager::CreateReason reason) {
  DCHECK(!GetLogFromHash(log_hash));

  // Insert a new log into |logs_| with the given |log_hash| to indicate that
  // this observer is now aware and keeping track of this log.
  std::unique_ptr<Log> log = std::make_unique<Log>();
  log->hash = std::string(log_hash);
  log->timestamp = std::string(log_timestamp);
  log->data = std::string(log_data);
  if (uma_log_type_.has_value()) {
    DCHECK_EQ(service_type_, MetricsServiceType::UMA);
    log->type = uma_log_type_;
  }

  // Immediately create a |kLogCreated| log event, along with the reason why the
  // log was created.
  log->events.push_back(
      CreateEventStruct(MetricsLogsEventManager::LogEvent::kLogCreated,
                        CreateReasonToString(reason)));

  indexed_logs_.emplace(log->hash, log.get());
  logs_.push_back(std::move(log));

  // Call all registered callbacks.
  notified_callbacks_.Notify();
}

void MetricsServiceObserver::OnLogEvent(MetricsLogsEventManager::LogEvent event,
                                        std::string_view log_hash,
                                        std::string_view message) {
  Log* log = GetLogFromHash(log_hash);

  // If this observer is not aware of any logs with the given |log_hash|, do
  // nothing. This may happen if this observer started observing after a log
  // was already created.
  if (!log)
    return;

  log->events.push_back(CreateEventStruct(event, message));

  // Call all registered callbacks.
  notified_callbacks_.Notify();
}

void MetricsServiceObserver::OnLogType(
    std::optional<MetricsLog::LogType> log_type) {
  uma_log_type_ = log_type;
}

bool MetricsServiceObserver::ExportLogsAsJson(bool include_log_proto_data,
                                              std::string* json_output) {
  base::Value::List logs_list;
  // Create and append to |logs_list| a base::Value for each log in |logs_|.
  for (const std::unique_ptr<Log>& log : logs_) {
    base::Value::Dict log_dict;

    if (log->type.has_value()) {
      DCHECK_EQ(service_type_, MetricsServiceType::UMA);
      log_dict.Set("type", LogTypeToString(log->type.value()));
    }
    log_dict.Set("hash", base::HexEncode(log->hash));
    log_dict.Set("timestamp", log->timestamp);

    if (include_log_proto_data) {
      log_dict.Set("data", base::Base64Encode(log->data));
    }

    log_dict.Set("size", static_cast<int>(log->data.length()));

    base::Value::List log_events_list;
    for (const Log::Event& event : log->events) {
      base::Value::Dict log_event_dict;
      log_event_dict.Set("event", EventToString(event.event));
      log_event_dict.Set("timestampMs", event.timestampMs);
      if (event.message.has_value())
        log_event_dict.Set("message", event.message.value());
      log_events_list.Append(std::move(log_event_dict));
    }
    log_dict.Set("events", std::move(log_events_list));

    logs_list.Append(std::move(log_dict));
  }

  // Create a last |dict| that contains all the logs and |service_type_|,
  // convert it to a JSON string, and write it to |json_output|.
  base::Value::Dict dict;
  dict.Set("logType", service_type_ == MetricsServiceType::UMA ? "UMA" : "UKM");
  dict.Set("logs", std::move(logs_list));

  JSONStringValueSerializer serializer(json_output);
  return serializer.Serialize(dict);
}

void MetricsServiceObserver::ExportLogsToFile(const base::FilePath& path) {
  std::string logs_data;
  bool success = ExportLogsAsJson(/*include_log_proto_data=*/true, &logs_data);
  DCHECK(success);
  if (!base::WriteFile(path, logs_data)) {
    LOG(ERROR) << "Failed to export logs to " << path << ": " << logs_data;
  }
}

base::CallbackListSubscription MetricsServiceObserver::AddNotifiedCallback(
    base::RepeatingClosure callback) {
  return notified_callbacks_.Add(callback);
}

MetricsServiceObserver::Log* MetricsServiceObserver::GetLogFromHash(
    std::string_view log_hash) {
  auto it = indexed_logs_.find(log_hash);
  return it != indexed_logs_.end() ? it->second : nullptr;
}

}  // namespace metrics
