// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_VOTER_BASE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_VOTER_BASE_H_

namespace performance_manager {

class Graph;

namespace execution_context_priority {

// Base class that voters can derive from to simplify their initialization in
// the ExecutionContextPriorityDecorator class.
class VoterBase {
 public:
  virtual ~VoterBase() = default;

  virtual void InitializeOnGraph(Graph* graph) = 0;
  virtual void TearDownOnGraph(Graph* graph) = 0;
};

}  // namespace execution_context_priority
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_VOTER_BASE_H_
