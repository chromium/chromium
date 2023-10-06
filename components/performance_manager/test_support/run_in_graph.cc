// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/run_in_graph.h"

#include "base/functional/function_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/performance_manager_impl.h"

namespace performance_manager {

void RunInGraph(base::FunctionRef<void()> on_graph_callback) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting(
                     [quit_loop = run_loop.QuitClosure(), &on_graph_callback] {
                       on_graph_callback();
                       quit_loop.Run();
                     }));
  run_loop.Run();
}

void RunInGraph(base::FunctionRef<void(Graph*)> on_graph_callback) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([quit_loop = run_loop.QuitClosure(),
                                             &on_graph_callback](Graph* graph) {
        on_graph_callback(graph);
        quit_loop.Run();
      }));
  run_loop.Run();
}

void RunInGraph(base::FunctionRef<void(GraphImpl*)> on_graph_callback) {
  base::RunLoop run_loop;
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindLambdaForTesting([quit_loop = run_loop.QuitClosure(),
                                  &on_graph_callback](GraphImpl* graph) {
        on_graph_callback(graph);
        quit_loop.Run();
      }));
  run_loop.Run();
}

}  // namespace performance_manager
