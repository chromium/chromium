// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FRAME_INPUT_STATE_DECORATOR_H_
#define COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FRAME_INPUT_STATE_DECORATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/node_attached_data.h"
#include "components/performance_manager/scenarios/browser_performance_scenarios.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"

namespace performance_manager {

class FrameInputStateObserver;

// FrameInputStateDecorator monitors the InputScenario updates and notifies the
// observers.
class FrameInputStateDecorator
    : public FrameNodeObserver,
      public GraphOwnedAndRegistered<FrameInputStateDecorator> {
 public:
  static constexpr base::TimeDelta kInactivityTimeoutForTyping =
      base::Seconds(3);

  FrameInputStateDecorator();
  FrameInputStateDecorator(const FrameInputStateDecorator&) = delete;
  FrameInputStateDecorator& operator=(const FrameInputStateDecorator&) = delete;
  ~FrameInputStateDecorator() override;

  // FrameNodeObserver implementation:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  void UpdateInputScenario(const FrameNode* frame_node, bool typing);

  void AddObserver(FrameInputStateObserver* observer);
  void RemoveObserver(FrameInputStateObserver* observer);

 private:
  // Let Data see the declaration of InputObserver.
  friend class Data;
  class InputObserver;

  base::ObserverList<FrameInputStateObserver> observers_;

 public:
  class Data : public ExternalNodeAttachedDataImpl<Data> {
   public:
    explicit Data(const FrameNode* frame_node);
    ~Data() override;
    InputScenario input_scenario() const { return input_scenario_; }
    void set_input_scenario(InputScenario input_scenario) {
      input_scenario_ = input_scenario;
    }

   private:
    InputScenario input_scenario_ = InputScenario::kNoInput;
    std::unique_ptr<InputObserver> input_observer_;
  };
};

class FrameInputStateObserver : public base::CheckedObserver {
 public:
  virtual void OnInputScenarioChanged(const FrameNode* frame_node) {}
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_DECORATORS_FRAME_INPUT_STATE_DECORATOR_H_
