// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/page_stability_monitor_delegate.h"

#include <memory>
#include <variant>

#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/renderer/actor/journal.h"
#include "chrome/renderer/actor/page_stability_metrics.h"
#include "components/page_content_annotations/core/page_stability_state.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/page_transition_types.h"

namespace actor {

PageStabilityMonitorDelegate::PageStabilityMonitorDelegate(TaskId task_id,
                                                           Journal& journal)
    : task_id_(task_id), journal_(journal) {}

PageStabilityMonitorDelegate::~PageStabilityMonitorDelegate() = default;

void PageStabilityMonitorDelegate::WillMoveToState(
    page_content_annotations::PageStabilityState state) {
  if (metrics_) {
    metrics_->WillMoveToState(state);
  }

  journal_entry_.reset();
  journal_entry_ = journal_->CreatePendingAsyncEntry(
      task_id_,
      absl::StrFormat(
          "PageStabilityState: %s",
          page_content_annotations::PageStabilityStateToString(state)),
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
            journal_entry_->Log("MonitorStartDelay",
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
            journal_entry_.reset();
          },
          [&](const page_content_annotations::InteractionContentfulPaintEvent&
                  e) {
            if (metrics_) {
              metrics_->OnInteractionContentfulPaint();
            }

            if (e.data.has_value()) {
              journal_->Log(
                  task_id_, "PaintStabilityMonitor: InteractionContentfulPaint",
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
            journal_->Log(
                task_id_, "PaintStabilityMonitor: InteractionContentfulPaint",
                JournalDetailsBuilder()
                    .Add("initial_painted_area", e.initial_painted_area)
                    .Build());
          },
          [&](const page_content_annotations::PaintStabilityDetectedEvent& e) {
            journal_->Log(
                task_id_, "PaintStabilityMonitor: Stability Detected",
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
            journal_->Log(task_id_, "PageStability: DidCommitProvisionalLoad",
                          JournalDetailsBuilder()
                              .Add("transition",
                                   ui::PageTransitionGetCoreTransitionString(
                                       e.transition))
                              .Build());
          },
          [&](const page_content_annotations::DidFailProvisionalLoadEvent&) {
            journal_->Log(task_id_, "DidFailProvisionalLoad", {});
          },
          [&](const page_content_annotations::DidSetPageLifecycleStateEvent&) {
            journal_->Log(task_id_, "PageStabilityMonitor Page Frozen", {});
          },
          [&](const page_content_annotations::NetworkIdleEvent&) {
            journal_->Log(task_id_,
                          "NetworkAndMainThreadStabilityMonitor::OnNetworkIdle",
                          {});
          },
          [&](const page_content_annotations::MainThreadIdleEvent&) {
            journal_->Log(
                task_id_,
                "NetworkAndMainThreadStabilityMonitor::OnMainThreadIdle", {});
          },
          [&](const page_content_annotations::
                  NetworkAndMainThreadStabilityMonitorCreatedEvent& e) {
            journal_->Log(task_id_,
                          "NetworkAndMainThreadStabilityMonitor: Created",
                          JournalDetailsBuilder()
                              .Add("requests_before", e.starting_request_count)
                              .Build());
          },
          [&](const page_content_annotations::
                  NetworkAndMainThreadStabilityMonitorStartedEvent& e) {
            journal_->Log(task_id_,
                          "NetworkAndMainThreadStabilityMonitor: WaitForStable",
                          JournalDetailsBuilder()
                              .Add("requests_after", e.after_request_count)
                              .Build());
          },
      },
      event);
}

base::TimeDelta PageStabilityMonitorDelegate::GetTimeoutDelay() const {
  return features::kGlicActorPageStabilityTimeout.Get();
}

base::TimeDelta PageStabilityMonitorDelegate::GetMinWait() const {
  return features::kGlicActorPageStabilityMinWait.Get();
}

// TODO(b/507143691): This is not based on data and should be revisited when
// histograms are available, or combined with other heuristics, e.g. pending
// interaction-attributed network requests.
base::TimeDelta PageStabilityMonitorDelegate::GetInitialPaintTimeout() const {
  return features::kActorPaintStabilityIntialPaintTimeout.Get();
}

// TODO(b/507143691): This is not based on data and should be revisited when
// histograms are available, or combined with other heuristics, e.g. pending
// interaction-attributed network requests.
base::TimeDelta PageStabilityMonitorDelegate::GetSubsequentPaintTimeout()
    const {
  return features::kActorPaintStabilitySubsequentPaintTimeout.Get();
}

}  // namespace actor
