// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check_op.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/scenarios/performance_scenario_observer.h"
#include "components/performance_manager/scenarios/performance_scenario_data.h"

namespace performance_manager {

PerformanceScenarioNotifier::PerformanceScenarioNotifier() = default;

PerformanceScenarioNotifier::~PerformanceScenarioNotifier() = default;

void PerformanceScenarioNotifier::AddGlobalObserver(
    PerformanceScenarioObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_observers_.AddObserver(observer);
}

void PerformanceScenarioNotifier::RemoveGlobalObserver(
    const PerformanceScenarioObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  global_observers_.RemoveObserver(observer);
}

void PerformanceScenarioNotifier::AddObserverForProcess(
    const ProcessNode* process_node,
    PerformanceScenarioObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(process_node);
  auto& data = PerformanceScenarioMemoryData::GetOrCreate(process_node);
  if (!data.observers) {
    data.observers = std::make_unique<ObserverList>();
  }
  data.observers->AddObserver(observer);
}

void PerformanceScenarioNotifier::RemoveObserverForProcess(
    const ProcessNode* process_node,
    const PerformanceScenarioObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(process_node);
  auto& data = PerformanceScenarioMemoryData::GetOrCreate(process_node);
  CHECK(data.observers);
  data.observers->RemoveObserver(observer);
}

PerformanceScenarioNotifier::ObserverList*
PerformanceScenarioNotifier::GetGlobalObservers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return &global_observers_;
}

PerformanceScenarioNotifier::ObserverList*
PerformanceScenarioNotifier::GetProcessObservers(
    const ProcessNode* process_node) {
  CHECK(process_node);
  return PerformanceScenarioMemoryData::GetOrCreate(process_node)
      .observers.get();
}

}  // namespace performance_manager
