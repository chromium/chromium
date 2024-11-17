// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_LOADING_SCENARIO_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_LOADING_SCENARIO_DATA_H_

#include <map>
#include <vector>

#include "components/performance_manager/graph/node_inline_data.h"

namespace performance_manager {

class ProcessNode;

// Counts of pages in each loading state. Per-process loading counts are stored
// inline in ProcessNode; global counts are stored in LoadingScenarioObserver.
class LoadingScenarioCounts : public NodeInlineData<LoadingScenarioCounts> {
 public:
  LoadingScenarioCounts() = default;
  ~LoadingScenarioCounts() = default;

  // Move-only.
  LoadingScenarioCounts(const LoadingScenarioCounts&) = delete;
  LoadingScenarioCounts& operator=(const LoadingScenarioCounts&) = delete;
  LoadingScenarioCounts(LoadingScenarioCounts&&) = default;
  LoadingScenarioCounts& operator=(LoadingScenarioCounts&&) = default;

  // Focused pages that are loading.
  size_t focused_loading_pages() const { return focused_loading_pages_; }

  // Visible pages (including focused) that are loading.
  size_t visible_loading_pages() const { return visible_loading_pages_; }

  // All pages (including focused and visible) that are loading.
  size_t loading_pages() const { return loading_pages_; }

  void IncrementLoadingPageCounts(bool visible, bool focused);
  void DecrementLoadingPageCounts(bool visible, bool focused);

 private:
  size_t focused_loading_pages_ = 0;
  size_t visible_loading_pages_ = 0;
  size_t loading_pages_ = 0;
};

// A cache of the number of frames hosted in each process in a page. Used to
// detect when the last frame in a given process is removed from the page.
// Stored inline in PageNode.
class LoadingScenarioPageFrameCounts
    : public NodeInlineData<LoadingScenarioPageFrameCounts> {
 public:
  LoadingScenarioPageFrameCounts();
  ~LoadingScenarioPageFrameCounts();

  // Move-only.
  LoadingScenarioPageFrameCounts(const LoadingScenarioPageFrameCounts&) =
      delete;
  LoadingScenarioPageFrameCounts& operator=(
      const LoadingScenarioPageFrameCounts&) = delete;
  LoadingScenarioPageFrameCounts(LoadingScenarioPageFrameCounts&&);
  LoadingScenarioPageFrameCounts& operator=(LoadingScenarioPageFrameCounts&&);

  // Increments the number of frames in this page hosted in `process_node`, and
  // returns the new value.
  size_t IncrementFrameCountForProcess(const ProcessNode* process_node);

  // Decrements the number of frames in this page hosted in `process_node`, and
  // returns the new value.
  size_t DecrementFrameCountForProcess(const ProcessNode* process_node);

  // Returns `true` iff any frame in this page is hosted in `process_node`.
  bool ProcessHasFramesInPage(const ProcessNode* process_node) const;

  // Returns all processes hosting at least one frame in this page.
  std::vector<const ProcessNode*> GetProcessesWithFramesInPage() const;

 private:
  std::map<const ProcessNode*, size_t> process_frame_counts_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SCENARIOS_LOADING_SCENARIO_DATA_H_
