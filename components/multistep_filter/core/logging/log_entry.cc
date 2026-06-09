// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/log_entry.h"

#include <string>
#include <string_view>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"

namespace multistep_filter {

namespace {

constexpr std::string_view LogEventTypeToString(LogEventType type) {
  switch (type) {
    case LogEventType::kNavigationStarted:
      return "NavigationStarted";
    case LogEventType::kUrlEligibilityCheck:
      return "UrlEligibilityCheck";
    case LogEventType::kAnnotationExtractionStarted:
      return "AnnotationExtractionStarted";
    case LogEventType::kAnnotationsExtracted:
      return "AnnotationsExtracted";
    case LogEventType::kSuggestionGenerationStarted:
      return "SuggestionGenerationStarted";
    case LogEventType::kNoSupportedTasks:
      return "NoSupportedTasks";
    case LogEventType::kNoRelevantAnnotations:
      return "NoRelevantAnnotations";
    case LogEventType::kServerRequestSent:
      return "ServerRequestSent";
    case LogEventType::kServerResponseReceived:
      return "ServerResponseReceived";
    case LogEventType::kSuggestionGenerated:
      return "SuggestionGenerated";
    case LogEventType::kSuggestionSuppressed:
      return "SuggestionSuppressed";
    case LogEventType::kSuggestionCleared:
      return "SuggestionCleared";
    case LogEventType::kSuggestionShown:
      return "SuggestionShown";
    case LogEventType::kSuggestionAccepted:
      return "SuggestionAccepted";
    case LogEventType::kSuggestionDismissed:
      return "SuggestionDismissed";
    case LogEventType::kSuggestionIgnored:
      return "SuggestionIgnored";
    case LogEventType::kServerRequestFailed:
      return "ServerRequestFailed";
    case LogEventType::kServerResponseMalformed:
      return "ServerResponseMalformed";
  }
  NOTREACHED();
}

}  // namespace

LogEntry::LogEntry(int64_t navigation_id,
                   LogEventType type,
                   std::string_view source_etld_plus_1)
    : navigation_id(navigation_id),
      event_type(type),
      source_etld_plus_1(source_etld_plus_1) {}

LogEntry::LogEntry(base::Time time,
                   int64_t navigation_id,
                   LogEventType type,
                   std::string_view source_etld_plus_1)
    : timestamp(time),
      navigation_id(navigation_id),
      event_type(type),
      source_etld_plus_1(source_etld_plus_1) {}

LogEntry::LogEntry(LogEntry&& other) noexcept = default;

LogEntry& LogEntry::operator=(LogEntry&& other) noexcept = default;

LogEntry::~LogEntry() = default;

LogEntry LogEntry::Clone() const {
  LogEntry clone(timestamp, navigation_id, event_type, source_etld_plus_1);
  clone.details = details.Clone();
  return clone;
}

base::Value LogEntry::ToValue() const {
  base::DictValue dict;
  dict.Set("timestamp", timestamp.InSecondsFSinceUnixEpoch());
  dict.Set("navigation_id", base::NumberToString(navigation_id));
  dict.Set("event_type", LogEventTypeToString(event_type));
  dict.Set("source_etld_plus_1", source_etld_plus_1);
  dict.Set("details", details.Clone());
  return base::Value(std::move(dict));
}

}  // namespace multistep_filter
