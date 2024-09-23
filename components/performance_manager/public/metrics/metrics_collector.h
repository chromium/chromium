// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_

#include <array>
#include <optional>
#include <set>

#include "base/numerics/clamped_math.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager {

extern const base::TimeDelta kMetricsReportDelayTimeout;
extern const int kDefaultFrequencyUkmEQTReported;

// The MetricsCollector is a graph observer that reports UMA/UKM.
class MetricsCollector : public FrameNode::ObserverDefaultImpl,
                         public GraphOwned,
                         public PageNode::ObserverDefaultImpl,
                         public ProcessNode::ObserverDefaultImpl {
 public:
  MetricsCollector();

  MetricsCollector(const MetricsCollector&) = delete;
  MetricsCollector& operator=(const MetricsCollector&) = delete;

  ~MetricsCollector() override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver implementation:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnLoadingStateChanged(const PageNode* page_node,
                             PageNode::LoadingState previous_state) override;
  void OnUkmSourceIdChanged(const PageNode* page_node) override;
  void OnMainFrameDocumentChanged(const PageNode* page_node) override;

  // ProcessNodeObserver implementation:
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

 protected:
  friend class MetricsReportRecordHolder;
  friend class UkmCollectionStateHolder;

  struct MetricsReportRecord {
    MetricsReportRecord();
    MetricsReportRecord(const MetricsReportRecord& other);
    GURL previous_url;
  };

  struct UkmCollectionState {
    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  };

 private:
  static MetricsReportRecord* GetMetricsReportRecord(const PageNode* page_node);
  static UkmCollectionState* GetUkmCollectionState(const PageNode* page_node);

  enum class PageLoadingState : size_t {
    // Loading, visible for the whole load.
    kLoadingVisible = 0,
    // Loading, hidden for the whole load.
    kLoadingHidden,
    // Loading, mix of visible and hidden.
    kLoadingMixed,
    // Not loading or reached quiescence after load.
    kQuiescent,
    kMaxValue = kQuiescent,
  };
  static constexpr size_t kNumLoadingStates =
      static_cast<size_t>(PageLoadingState::kMaxValue) + 1;

  // (Un)registers the various node observer flavors of this object with the
  // graph. These are invoked by OnPassedToGraph and OnTakenFromGraph, but
  // hoisted to their own functions for testing.
  void RegisterObservers(Graph* graph);
  void UnregisterObservers(Graph* graph);

  bool ShouldReportMetrics(const PageNode* page_node);
  void UpdateUkmSourceIdForPage(const PageNode* page_node,
                                ukm::SourceId ukm_source_id);

  void OnProcessDestroyed(const ProcessNode* process_node);

  // Decrements the count for `old_state` (which should be nullopt for a new
  // page with no previous state), and increments the count for `new_state`
  // (nullopt for pages being destroyed).
  void UpdateLoadingPageCounts(std::optional<PageLoadingState> old_state,
                               std::optional<PageLoadingState> new_state);
  void RecordLoadingAndQuiescentPageCount() const;

  // Timer used to schedule QuiescentPageCount metrics.
  base::RepeatingTimer page_loading_state_timer_;

  // Count of PageNodes, split by loading state.
  std::array<base::ClampedNumeric<size_t>, kNumLoadingStates>
      loading_page_counts_;

  // Nodes in visibility state kMixed, since this can't be calculated from the
  // PageNode alone.
  std::set<const PageNode*> mixed_state_pages_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_METRICS_METRICS_COLLECTOR_H_
