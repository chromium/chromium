// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/chrome_page_stability_monitor_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/actor/renderer/renderer_page_stability_metrics.h"

namespace actor {

ChromePageStabilityMonitorDelegate::ChromePageStabilityMonitorDelegate(
    TaskId task_id,
    Journal& journal,
    const Thresholds& thresholds)
    : PageStabilityMonitorDelegate(task_id, thresholds), journal_(journal) {}

ChromePageStabilityMonitorDelegate::~ChromePageStabilityMonitorDelegate() =
    default;

void ChromePageStabilityMonitorDelegate::LogEvent(
    mojom::JournalEntryType type,
    std::string_view event_name,
    std::vector<mojom::JournalDetailsPtr> details) {
  switch (type) {
    case mojom::JournalEntryType::kBegin:
      async_journal_entry_ = journal_->CreatePendingAsyncEntry(
          task_id(), event_name, std::move(details));
      break;
    case mojom::JournalEntryType::kEnd:
      async_journal_entry_.reset();
      break;
    case mojom::JournalEntryType::kInstant:
      if (async_journal_entry_) {
        async_journal_entry_->Log(event_name, std::move(details));
      } else {
        journal_->Log(task_id(), event_name, std::move(details));
      }
      break;
  }
}

std::unique_ptr<PageStabilityMetrics>
ChromePageStabilityMonitorDelegate::CreateMetrics() {
  return std::make_unique<RendererPageStabilityMetrics>();
}

}  // namespace actor
