// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/scenarios/loading_scenario_data.h"

#include <map>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/numerics/checked_math.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager {

namespace {

// Increments `num` in-place, and CHECK's on overflow.
void CheckIncrement(size_t& num) {
  num = base::CheckAdd(num, 1).ValueOrDie();
}

// Decrements `num` in-place, and CHECK's on underflow.
void CheckDecrement(size_t& num) {
  num = base::CheckSub(num, 1).ValueOrDie();
}

}  // namespace

void LoadingScenarioCounts::IncrementLoadingPageCounts(bool visible,
                                                       bool focused) {
  CheckIncrement(loading_pages_);
  if (visible) {
    CheckIncrement(visible_loading_pages_);
  }
  if (focused) {
    CheckIncrement(focused_loading_pages_);
  }
}

void LoadingScenarioCounts::DecrementLoadingPageCounts(bool visible,
                                                       bool focused) {
  CheckDecrement(loading_pages_);
  if (visible) {
    CheckDecrement(visible_loading_pages_);
  }
  if (focused) {
    CheckDecrement(focused_loading_pages_);
  }
}

LoadingScenarioPageFrameCounts::LoadingScenarioPageFrameCounts() = default;

LoadingScenarioPageFrameCounts::~LoadingScenarioPageFrameCounts() = default;

LoadingScenarioPageFrameCounts::LoadingScenarioPageFrameCounts(
    LoadingScenarioPageFrameCounts&&) = default;

LoadingScenarioPageFrameCounts& LoadingScenarioPageFrameCounts::operator=(
    LoadingScenarioPageFrameCounts&&) = default;

size_t LoadingScenarioPageFrameCounts::IncrementFrameCountForProcess(
    const ProcessNode* process_node) {
  auto [it, _] = process_frame_counts_.try_emplace(process_node, 0u);
  CheckIncrement(it->second);
  return it->second;
}

size_t LoadingScenarioPageFrameCounts::DecrementFrameCountForProcess(
    const ProcessNode* process_node) {
  auto it = process_frame_counts_.find(process_node);
  CHECK(it != process_frame_counts_.end());
  CheckDecrement(it->second);
  if (it->second == 0) {
    process_frame_counts_.erase(it);
    return 0;
  }
  return it->second;
}

bool LoadingScenarioPageFrameCounts::ProcessHasFramesInPage(
    const ProcessNode* process_node) const {
  return base::Contains(process_frame_counts_, process_node);
}

std::vector<const ProcessNode*>
LoadingScenarioPageFrameCounts::GetProcessesWithFramesInPage() const {
  std::vector<const ProcessNode*> process_nodes;
  process_nodes.reserve(process_frame_counts_.size());
  for (const auto& [process_node, frame_count] : process_frame_counts_) {
    CHECK_GT(frame_count, 0u);
    process_nodes.push_back(process_node);
  }
  return process_nodes;
}

}  // namespace performance_manager
