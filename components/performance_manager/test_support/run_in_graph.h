// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RUN_IN_GRAPH_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RUN_IN_GRAPH_H_

#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"

namespace performance_manager {

class Graph;
class GraphImpl;

// Helper functions for running a task on the graph, and waiting for it to
// complete. `on_graph_callback` will be called on the PM sequence.
void RunInGraph(base::FunctionRef<void()> on_graph_callback);
void RunInGraph(base::FunctionRef<void(Graph*)> on_graph_callback);
void RunInGraph(base::FunctionRef<void(GraphImpl*)> on_graph_callback);

// These versions of RunInGraph can be used to start async operations from
// inside `on_graph_callback`. When done they must invoke the closure, which
// is the QuitClosure() of a RunLoop.
void RunInGraph(base::FunctionRef<void(base::OnceClosure)> on_graph_callback);
void RunInGraph(
    base::FunctionRef<void(base::OnceClosure, Graph*)> on_graph_callback);
void RunInGraph(
    base::FunctionRef<void(base::OnceClosure, GraphImpl*)> on_graph_callback);

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RUN_IN_GRAPH_H_
