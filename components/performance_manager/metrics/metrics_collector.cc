// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/metrics_collector.h"

#include <array>
#include <optional>
#include <set>
#include <string>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "content/public/common/process_type.h"

namespace performance_manager {

namespace {

void RecordProcessLifetime(const std::string& histogram_name,
                           base::TimeDelta lifetime) {
  base::UmaHistogramCustomTimes(histogram_name, lifetime, base::Seconds(1),
                                base::Days(1), 100);
}

void RecordShortProcessLifetime(const std::string& histogram_name,
                                base::TimeDelta lifetime) {
  base::UmaHistogramLongTimes(histogram_name, lifetime);
}

void OnRendererDestroyed(const ProcessNode* process_node,
                         base::TimeDelta lifetime) {
  RecordProcessLifetime("Renderer.ProcessLifetime3", lifetime);

  ProcessNode::ContentTypes content_types =
      process_node->GetHostedContentTypes();
  if (content_types.Has(ProcessNode::ContentType::kExtension)) {
    RecordProcessLifetime("Renderer.ProcessLifetime3.Extension", lifetime);
  } else if (!content_types.Has(ProcessNode::ContentType::kNavigatedFrame)) {
    if (content_types.Has(ProcessNode::ContentType::kWorker)) {
      RecordProcessLifetime("Renderer.ProcessLifetime3.Worker", lifetime);
    } else if (content_types.Has(ProcessNode::ContentType::kMainFrame) ||
               content_types.Has(ProcessNode::ContentType::kSubframe)) {
      RecordProcessLifetime("Renderer.ProcessLifetime3.Speculative", lifetime);
    } else {
      RecordProcessLifetime("Renderer.ProcessLifetime3.Empty", lifetime);
    }
  } else if (content_types.Has(ProcessNode::ContentType::kMainFrame)) {
    RecordProcessLifetime("Renderer.ProcessLifetime3.MainFrame", lifetime);
  } else if (content_types.Has(ProcessNode::ContentType::kAd)) {
    RecordProcessLifetime("Renderer.ProcessLifetime3.Subframe_Ad", lifetime);
  } else if (content_types.Has(ProcessNode::ContentType::kSubframe)) {
    RecordProcessLifetime("Renderer.ProcessLifetime3.Subframe_NoAd", lifetime);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

bool LoadingStateIsQuiescent(PageNode::LoadingState loading_state) {
  switch (loading_state) {
    case PageNode::LoadingState::kLoadingNotStarted:
    case PageNode::LoadingState::kLoadingTimedOut:
    case PageNode::LoadingState::kLoadedIdle:
      return true;
    case PageNode::LoadingState::kLoading:
    case PageNode::LoadingState::kLoadedBusy:
      return false;
  }
  NOTREACHED();
}

}  // namespace

class MetricsReportRecordHolder
    : public ExternalNodeAttachedDataImpl<MetricsReportRecordHolder> {
 public:
  explicit MetricsReportRecordHolder(const PageNode* unused_page_node) {}
  ~MetricsReportRecordHolder() override = default;
  MetricsCollector::MetricsReportRecord metrics_report_record;
};

class UkmCollectionStateHolder
    : public ExternalNodeAttachedDataImpl<UkmCollectionStateHolder> {
 public:
  explicit UkmCollectionStateHolder(const PageNode* unused_page_node) {}
  ~UkmCollectionStateHolder() override = default;
  MetricsCollector::UkmCollectionState ukm_collection_state;
};

// Delay the metrics report from for 5 minutes from when the main frame
// navigation is committed.
const base::TimeDelta kMetricsReportDelayTimeout = base::Minutes(5);

const char kTabNavigationWithSameOriginTabHistogramName[] =
    "Tabs.NewNavigationWithSameOriginTab";

const int kDefaultFrequencyUkmEQTReported = 5u;

MetricsCollector::MetricsCollector() = default;

MetricsCollector::~MetricsCollector() = default;

void MetricsCollector::OnPassedToGraph(Graph* graph) {
  RegisterObservers(graph);

  loading_page_counts_.fill(0);

  // Unretained is safe because `this` owns the timer.
  page_loading_state_timer_.Start(
      FROM_HERE, base::Seconds(30),
      base::BindRepeating(&MetricsCollector::RecordLoadingAndQuiescentPageCount,
                          base::Unretained(this)));
}

void MetricsCollector::OnTakenFromGraph(Graph* graph) {
  page_loading_state_timer_.Stop();
  UnregisterObservers(graph);
}

void MetricsCollector::OnPageNodeAdded(const PageNode* page_node) {
  // Record the initial state of the page.
  DCHECK(!base::Contains(mixed_state_pages_, page_node));
  const auto loading_state =
      LoadingStateIsQuiescent(page_node->GetLoadingState())
          ? PageLoadingState::kQuiescent
          : (page_node->IsVisible() ? PageLoadingState::kLoadingVisible
                                    : PageLoadingState::kLoadingHidden);
  UpdateLoadingPageCounts(std::nullopt, loading_state);
}

void MetricsCollector::OnBeforePageNodeRemoved(const PageNode* page_node) {
  // Remove the page from all counts.
  PageLoadingState previous_state;
  if (LoadingStateIsQuiescent(page_node->GetLoadingState())) {
    DCHECK(!base::Contains(mixed_state_pages_, page_node));
    previous_state = PageLoadingState::kQuiescent;
  } else {
    size_t erased = mixed_state_pages_.erase(page_node);
    previous_state =
        erased ? PageLoadingState::kLoadingMixed
               : (page_node->IsVisible() ? PageLoadingState::kLoadingVisible
                                         : PageLoadingState::kLoadingHidden);
  }
  UpdateLoadingPageCounts(previous_state, std::nullopt);
}

void MetricsCollector::OnIsVisibleChanged(const PageNode* page_node) {
  if (LoadingStateIsQuiescent(page_node->GetLoadingState())) {
    return;
  }
  // Change of visibility makes the state kLoadingMixed if it wasn't already.
  const auto [_, inserted] = mixed_state_pages_.insert(page_node);
  if (inserted) {
    const auto previous_state = page_node->IsVisible()
                                    ? PageLoadingState::kLoadingHidden
                                    : PageLoadingState::kLoadingVisible;
    UpdateLoadingPageCounts(previous_state, PageLoadingState::kLoadingMixed);
  }
}

void MetricsCollector::OnLoadingStateChanged(
    const PageNode* page_node,
    PageNode::LoadingState previous_state) {
  const bool is_quiescent =
      LoadingStateIsQuiescent(page_node->GetLoadingState());
  const bool was_quiescent = LoadingStateIsQuiescent(previous_state);
  if (is_quiescent == was_quiescent) {
    return;
  }
  PageLoadingState old_state;
  PageLoadingState new_state;
  if (is_quiescent) {
    size_t erased = mixed_state_pages_.erase(page_node);
    old_state =
        erased ? PageLoadingState::kLoadingMixed
               : (page_node->IsVisible() ? PageLoadingState::kLoadingVisible
                                         : PageLoadingState::kLoadingHidden);
    new_state = PageLoadingState::kQuiescent;
  } else {
    DCHECK(!base::Contains(mixed_state_pages_, page_node));
    old_state = PageLoadingState::kQuiescent;
    new_state = page_node->IsVisible() ? PageLoadingState::kLoadingVisible
                                       : PageLoadingState::kLoadingHidden;
  }
  UpdateLoadingPageCounts(old_state, new_state);
}

void MetricsCollector::OnUkmSourceIdChanged(const PageNode* page_node) {
  ukm::SourceId ukm_source_id = page_node->GetUkmSourceID();
  UpdateUkmSourceIdForPage(page_node, ukm_source_id);
}

void MetricsCollector::OnMainFrameDocumentChanged(const PageNode* page_node) {
  bool found_same_origin_page = false;
  auto* record = GetMetricsReportRecord(page_node);
  if (!page_node->GetMainFrameUrl().SchemeIsHTTPOrHTTPS() ||
      url::IsSameOriginWith(record->previous_url,
                            page_node->GetMainFrameUrl())) {
    record->previous_url = page_node->GetMainFrameUrl();
    return;
  }

  for (const PageNode* page : GetOwningGraph()->GetAllPageNodes()) {
    if (page != page_node) {
      if (page->GetBrowserContextID() == page_node->GetBrowserContextID() &&
          url::IsSameOriginWith(page->GetMainFrameUrl(),
                                page_node->GetMainFrameUrl())) {
        found_same_origin_page = true;
        break;
      }
    }
  }
  record->previous_url = page_node->GetMainFrameUrl();
  base::UmaHistogramBoolean(kTabNavigationWithSameOriginTabHistogramName,
                            found_same_origin_page);
}

void MetricsCollector::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  // Ignore process creation.
  if (!process_node->GetExitStatus().has_value()) {
    return;
  }

  OnProcessDestroyed(process_node);
}

void MetricsCollector::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  // If the ProcessNode is destroyed with a valid process handle, consider this
  // the end of the process' life.
  if (process_node->GetProcess().IsValid()) {
    OnProcessDestroyed(process_node);
  }
}

// static
MetricsCollector::MetricsReportRecord* MetricsCollector::GetMetricsReportRecord(
    const PageNode* page_node) {
  auto* holder = MetricsReportRecordHolder::GetOrCreate(page_node);
  return &holder->metrics_report_record;
}

// static
MetricsCollector::UkmCollectionState* MetricsCollector::GetUkmCollectionState(
    const PageNode* page_node) {
  auto* holder = UkmCollectionStateHolder::GetOrCreate(page_node);
  return &holder->ukm_collection_state;
}

void MetricsCollector::RegisterObservers(Graph* graph) {
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void MetricsCollector::UnregisterObservers(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
}

bool MetricsCollector::ShouldReportMetrics(const PageNode* page_node) {
  return page_node->GetTimeSinceLastNavigation() > kMetricsReportDelayTimeout;
}

void MetricsCollector::UpdateUkmSourceIdForPage(const PageNode* page_node,
                                                ukm::SourceId ukm_source_id) {
  auto* state = GetUkmCollectionState(page_node);
  state->ukm_source_id = ukm_source_id;
}

MetricsCollector::MetricsReportRecord::MetricsReportRecord() = default;

MetricsCollector::MetricsReportRecord::MetricsReportRecord(
    const MetricsReportRecord& other) = default;

void MetricsCollector::OnProcessDestroyed(const ProcessNode* process_node) {
  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks launch_time = process_node->GetLaunchTime();

  if (launch_time.is_null()) {
    // Terminating a process quickly after initiating its launch (for example
    // with FastShutdownIfPossible()) may result in receiving
    // RenderProcessHostObserver::RenderProcessExited() without a corresponding
    // RenderProcessHostCreationObserver::OnRenderProcessHostCreated(). In this
    // case, GetLaunchTime() won't be set. It's correct to report a lifetime of
    // 0 in this case.
    launch_time = now;
  }

  const base::TimeDelta lifetime = now - launch_time;

  if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER) {
    OnRendererDestroyed(process_node, lifetime);
  } else if (process_node->GetProcessType() == content::PROCESS_TYPE_UTILITY) {
    // Utility processes are known to often have short lifetimes. There are
    // exceptions like the network service that could be broken out later if the
    // data suggests it's necessary.
    RecordShortProcessLifetime("ChildProcess.ProcessLifetime.Utility",
                               lifetime);
  }
}

void MetricsCollector::UpdateLoadingPageCounts(
    std::optional<PageLoadingState> old_state,
    std::optional<PageLoadingState> new_state) {
  CHECK(old_state != new_state);
  if (old_state.has_value()) {
    loading_page_counts_.at(static_cast<size_t>(old_state.value())) -= 1;
  }
  if (new_state.has_value()) {
    loading_page_counts_.at(static_cast<size_t>(new_state.value())) += 1;
  }
}

void MetricsCollector::RecordLoadingAndQuiescentPageCount() const {
  // Record all loading state counts.
  constexpr char kLoadingPageCountHistogram[] =
      "PerformanceManager.LoadingNotQuiescentPageCount";
  base::ClampedNumeric<size_t> total_count = 0;
  for (size_t i = 0; i < loading_page_counts_.size(); ++i) {
    const auto loading_state = static_cast<PageLoadingState>(i);
    CHECK_LE(loading_state, PageLoadingState::kMaxValue);
    const auto count = loading_page_counts_.at(i);
    const char* visibility_string = [loading_state]() -> const char* {
      switch (loading_state) {
        case PageLoadingState::kLoadingVisible:
          return ".Visible";
        case PageLoadingState::kLoadingHidden:
          return ".Hidden";
        case PageLoadingState::kLoadingMixed:
          return ".MixedVisibility";
        case PageLoadingState::kQuiescent:
          // Don't log here, not loading.
          return nullptr;
      }
      NOTREACHED();
    }();
    if (visibility_string) {
      base::UmaHistogramCounts1000(
          base::StrCat({kLoadingPageCountHistogram, visibility_string}), count);
      total_count += count;
    }
  }
  base::UmaHistogramCounts1000(
      base::StrCat({kLoadingPageCountHistogram, ".All"}), total_count);

  // Record quiescent state count.
  base::UmaHistogramCounts1000("PerformanceManager.QuiescentPageCount",
                               loading_page_counts_.at(static_cast<size_t>(
                                   PageLoadingState::kQuiescent)));
}

}  // namespace performance_manager
