// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_page_handler.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router.h"

namespace multistep_filter_internals {

namespace {

std::string_view LogEventTypeToString(multistep_filter::LogEventType type) {
  switch (type) {
    case multistep_filter::LogEventType::kNavigationStarted:
      return "Navigation Started";
    case multistep_filter::LogEventType::kUrlEligibilityCheck:
      return "Url Eligibility Check";
    case multistep_filter::LogEventType::kAnnotationExtractionStarted:
      return "Annotation Extraction Started";
    case multistep_filter::LogEventType::kAnnotationsExtracted:
      return "Annotations Extracted";
    case multistep_filter::LogEventType::kSuggestionGenerationStarted:
      return "Suggestion Generation Started";
    case multistep_filter::LogEventType::kNoSupportedTasks:
      return "No Supported Tasks";
    case multistep_filter::LogEventType::kNoRelevantAnnotations:
      return "No Relevant Annotations";
    case multistep_filter::LogEventType::kServerRequestSent:
      return "Server Request Sent";
    case multistep_filter::LogEventType::kServerResponseReceived:
      return "Server Response Received";
    case multistep_filter::LogEventType::kSuggestionGenerated:
      return "Suggestion Generated";
    case multistep_filter::LogEventType::kSuggestionSuppressed:
      return "Suggestion Suppressed";
    case multistep_filter::LogEventType::kSuggestionCleared:
      return "Suggestion Cleared";
    case multistep_filter::LogEventType::kSuggestionShown:
      return "Suggestion Shown";
    case multistep_filter::LogEventType::kSuggestionAccepted:
      return "Suggestion Accepted";
    case multistep_filter::LogEventType::kSuggestionDismissed:
      return "Suggestion Dismissed";
    case multistep_filter::LogEventType::kSuggestionIgnored:
      return "Suggestion Ignored";
    case multistep_filter::LogEventType::kServerRequestFailed:
      return "Server Request Failed";
    case multistep_filter::LogEventType::kServerResponseMalformed:
      return "Server Response Malformed";
  }
  NOTREACHED();
}

std::string ConvertDetailsToString(const base::DictValue& dict) {
  std::string result;
  for (auto [key, value] : dict) {
    if (!result.empty()) {
      result += ", ";
    }
    if (value.is_string()) {
      base::StrAppend(&result, {key, ": ", value.GetString()});
    } else if (value.is_bool()) {
      base::StrAppend(&result, {key, ": ", value.GetBool() ? "true" : "false"});
    } else if (value.is_int()) {
      base::StrAppend(&result,
                      {key, ": ", base::NumberToString(value.GetInt())});
    } else {
      base::StrAppend(&result, {key, ": (unsupported type)"});
    }
  }
  return result;
}

mojom::LogEntryPtr ConvertToMojo(const multistep_filter::LogEntry& entry) {
  mojom::LogEntryPtr mojo_entry = mojom::LogEntry::New();
  mojo_entry->timestamp = entry.timestamp;
  mojo_entry->event_type = LogEventTypeToString(entry.event_type);
  mojo_entry->source_etld_plus_1 = entry.source_etld_plus_1;
  mojo_entry->navigation_id = entry.navigation_id;
  mojo_entry->details = ConvertDetailsToString(entry.details);
  return mojo_entry;
}

}  // namespace

MultistepFilterInternalsPageHandler::MultistepFilterInternalsPageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page,
    multistep_filter::MultistepFilterLogRouter* log_router)
    : log_router_(log_router),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  if (log_router_) {
    log_router_observation_.Observe(log_router_);
  }
}

MultistepFilterInternalsPageHandler::~MultistepFilterInternalsPageHandler() =
    default;

void MultistepFilterInternalsPageHandler::GetBufferedLogs(
    GetBufferedLogsCallback callback) {
  if (!log_router_) {
    std::move(callback).Run({});
    return;
  }
  std::vector<multistep_filter::LogEntry> buffered_logs =
      log_router_->GetBufferedLogs();
  std::vector<mojom::LogEntryPtr> mojo_logs;
  mojo_logs.reserve(buffered_logs.size());
  for (const multistep_filter::LogEntry& entry : buffered_logs) {
    mojo_logs.push_back(ConvertToMojo(entry));
  }
  std::move(callback).Run(std::move(mojo_logs));
}

void MultistepFilterInternalsPageHandler::OnLogEntryAdded(
    const multistep_filter::LogEntry& entry) {
  page_->OnLogEntryAdded(ConvertToMojo(entry));
}

void MultistepFilterInternalsPageHandler::OnLogRouterShutdown() {
  log_router_observation_.Reset();
  log_router_ = nullptr;
}

}  // namespace multistep_filter_internals
