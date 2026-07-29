// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/page_stability_monitor_delegate.h"

#include <memory>
#include <utility>
#include <variant>

#include "base/time/time.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/core/page_stability_metrics.h"
#include "components/actor/core/task_id.h"
#include "components/page_content_annotations/core/page_stability_state.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/page_transition_types.h"

namespace actor {

PageStabilityMonitorDelegate::PageStabilityMonitorDelegate(
    TaskId task_id,
    const Thresholds& thresholds)
    : task_id_(task_id), thresholds_(thresholds) {}

PageStabilityMonitorDelegate::~PageStabilityMonitorDelegate() = default;

void PageStabilityMonitorDelegate::WillMoveToState(
    page_content_annotations::PageStabilityState state) {
  if (metrics_) {
    metrics_->WillMoveToState(state);
  }

  // End the previous state before starting the new one.
  if (active_state_event_name_.has_value()) {
    LogEvent(mojom::JournalEntryType::kEnd, active_state_event_name_.value(),
             {});
    active_state_event_name_.reset();
  }

  active_state_event_name_ = absl::StrFormat(
      "PageStabilityState: %s",
      page_content_annotations::PageStabilityStateToString(state));
  LogEvent(mojom::JournalEntryType::kBegin, active_state_event_name_.value(),
           {});
}

void PageStabilityMonitorDelegate::OnEvent(
    const page_content_annotations::PageStabilityEvent& event) {
  std::visit(
      absl::Overload{
          [&](const page_content_annotations::PageStabilityMonitorStartEvent&) {
            metrics_ = std::make_unique<PageStabilityMetrics>();
            metrics_->Start();
          },
          [&](const page_content_annotations::
                  PageStabilityMonitorStartDelayEvent& e) {
            LogEvent(mojom::JournalEntryType::kInstant, "MonitorStartDelay",
                     JournalDetailsBuilder()
                         .Add("delay", e.delay.InMilliseconds())
                         .Build());
          },
          [&](const page_content_annotations::PageStabilityMonitorStopEvent&) {
            if (metrics_) {
              metrics_->Flush();
            }
          },
          [&](const page_content_annotations::
                  PageStabilityMonitorTearDownEvent&) {
            if (active_state_event_name_.has_value()) {
              LogEvent(mojom::JournalEntryType::kEnd,
                       active_state_event_name_.value(), {});
              active_state_event_name_.reset();
            }
          },
          [&](const page_content_annotations::InteractionContentfulPaintEvent&
                  e) {
            if (metrics_) {
              metrics_->OnInteractionContentfulPaint();
            }

            if (e.data.has_value()) {
              LogEvent(
                  mojom::JournalEntryType::kInstant,
                  "PaintStabilityMonitor: InteractionContentfulPaint",
                  JournalDetailsBuilder()
                      .Add("total_painted_area", e.data->total_painted_area)
                      .Add("new_painted_area", e.data->new_painted_area)
                      .Add("was_stability_reached",
                           e.data->was_stability_reached)
                      .Build());
            }
          },
          [&](const page_content_annotations::PaintStabilityMonitorStartedEvent&
                  e) {
            LogEvent(mojom::JournalEntryType::kInstant,
                     "PaintStabilityMonitor: InteractionContentfulPaint",
                     JournalDetailsBuilder()
                         .Add("initial_painted_area", e.initial_painted_area)
                         .Build());
          },
          [&](const page_content_annotations::PaintStabilityDetectedEvent& e) {
            LogEvent(mojom::JournalEntryType::kInstant,
                     "PaintStabilityMonitor: Stability Detected",
                     JournalDetailsBuilder()
                         .Add("total_painted_area", e.total_painted_area)
                         .Add("is_waiting_for_stable", e.is_waiting_for_stable)
                         .Build());
          },
          [&](const page_content_annotations::PaintStabilityReachedEvent&) {
            if (metrics_) {
              metrics_->OnPaintStabilityReached();
            }
          },
          [&](const page_content_annotations::NetworkAndMainThreadIdleEvent&) {
            if (metrics_) {
              metrics_->OnNetworkAndMainThreadIdle();
            }
          },
          [&](const page_content_annotations::DidCommitProvisionalLoadEvent&
                  e) {
            LogEvent(mojom::JournalEntryType::kInstant,
                     "PageStability: DidCommitProvisionalLoad",
                     JournalDetailsBuilder()
                         .Add("transition",
                              ui::PageTransitionGetCoreTransitionString(
                                  e.transition))
                         .Build());
          },
          [&](const page_content_annotations::DidFailProvisionalLoadEvent&) {
            LogEvent(mojom::JournalEntryType::kInstant,
                     "DidFailProvisionalLoad", {});
          },
          [&](const page_content_annotations::DidSetPageLifecycleStateEvent&) {
            LogEvent(mojom::JournalEntryType::kInstant,
                     "PageStabilityMonitor Page Frozen", {});
          },
          [&](const page_content_annotations::NetworkIdleEvent&) {
            LogEvent(mojom::JournalEntryType::kInstant,
                     "NetworkAndMainThreadStabilityMonitor::OnNetworkIdle", {});
          },
          [&](const page_content_annotations::MainThreadIdleEvent&) {
            LogEvent(mojom::JournalEntryType::kInstant,
                     "NetworkAndMainThreadStabilityMonitor::OnMainThreadIdle",
                     {});
          },
          [&](const page_content_annotations::
                  NetworkAndMainThreadStabilityMonitorCreatedEvent& e) {
            LogEvent(mojom::JournalEntryType::kInstant,
                     "NetworkAndMainThreadStabilityMonitor: Created",
                     JournalDetailsBuilder()
                         .Add("requests_before", e.starting_request_count)
                         .Build());
          },
          [&](const page_content_annotations::
                  NetworkAndMainThreadStabilityMonitorStartedEvent& e) {
            LogEvent(mojom::JournalEntryType::kInstant,
                     "NetworkAndMainThreadStabilityMonitor: WaitForStable",
                     JournalDetailsBuilder()
                         .Add("requests_after", e.after_request_count)
                         .Build());
          },
      },
      event);
}

base::TimeDelta PageStabilityMonitorDelegate::GetTimeoutDelay() const {
  return thresholds_.timeout_delay;
}

base::TimeDelta PageStabilityMonitorDelegate::GetMinWait() const {
  return thresholds_.min_wait;
}

// TODO(b/507143691): This is not based on data and should be revisited when
// histograms are available, or combined with other heuristics, e.g. pending
// interaction-attributed network requests.
base::TimeDelta PageStabilityMonitorDelegate::GetInitialPaintTimeout() const {
  return thresholds_.initial_paint_timeout;
}

// TODO(b/507143691): This is not based on data and should be revisited when
// histograms are available, or combined with other heuristics, e.g. pending
// interaction-attributed network requests.
base::TimeDelta PageStabilityMonitorDelegate::GetSubsequentPaintTimeout()
    const {
  return thresholds_.subsequent_paint_timeout;
}

}  // namespace actor
