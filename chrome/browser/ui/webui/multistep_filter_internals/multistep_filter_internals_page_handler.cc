// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals_page_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router.h"

namespace multistep_filter_internals {

namespace {

mojom::LogEventType ToMojoLogEventType(multistep_filter::LogEventType type) {
  switch (type) {
    case multistep_filter::LogEventType::kNavigationStarted:
      return mojom::LogEventType::kNavigationStarted;
    case multistep_filter::LogEventType::kUrlEligibilityCheck:
      return mojom::LogEventType::kUrlEligibilityCheck;
    case multistep_filter::LogEventType::kAnnotationExtractionStarted:
      return mojom::LogEventType::kAnnotationExtractionStarted;
    case multistep_filter::LogEventType::kAnnotationsExtracted:
      return mojom::LogEventType::kAnnotationsExtracted;
    case multistep_filter::LogEventType::kSuggestionGenerationStarted:
      return mojom::LogEventType::kSuggestionGenerationStarted;
    case multistep_filter::LogEventType::kNoSupportedTasks:
      return mojom::LogEventType::kNoSupportedTasks;
    case multistep_filter::LogEventType::kNoRelevantAnnotations:
      return mojom::LogEventType::kNoRelevantAnnotations;
    case multistep_filter::LogEventType::kServerRequestSent:
      return mojom::LogEventType::kServerRequestSent;
    case multistep_filter::LogEventType::kServerResponseReceived:
      return mojom::LogEventType::kServerResponseReceived;
    case multistep_filter::LogEventType::kSuggestionGenerated:
      return mojom::LogEventType::kSuggestionGenerated;
    case multistep_filter::LogEventType::kSuggestionSuppressed:
      return mojom::LogEventType::kSuggestionSuppressed;
    case multistep_filter::LogEventType::kSuggestionCleared:
      return mojom::LogEventType::kSuggestionCleared;
    case multistep_filter::LogEventType::kUiShown:
      return mojom::LogEventType::kUiShown;
    case multistep_filter::LogEventType::kUiAccepted:
      return mojom::LogEventType::kUiAccepted;
    case multistep_filter::LogEventType::kUiDismissed:
      return mojom::LogEventType::kUiDismissed;
    case multistep_filter::LogEventType::kServerRequestFailed:
      return mojom::LogEventType::kServerRequestFailed;
    case multistep_filter::LogEventType::kServerResponseMalformed:
      return mojom::LogEventType::kServerResponseMalformed;
  }
  NOTREACHED();
}

mojom::LogEntryPtr ConvertToMojo(const multistep_filter::LogEntry& entry) {
  mojom::LogEntryPtr mojo_entry = mojom::LogEntry::New();
  mojo_entry->timestamp = entry.timestamp;
  mojo_entry->event_type = ToMojoLogEventType(entry.event_type);
  mojo_entry->source_etld_plus_1 = entry.source_etld_plus_1;
  mojo_entry->navigation_id = entry.navigation_id;
  mojo_entry->details = entry.details.Clone();
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
