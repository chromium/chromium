// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/run_in_graph.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/performance_manager_impl.h"

namespace performance_manager {

void RunInGraph(base::FunctionRef<void()> on_graph_callback) {
  RunInGraph([&on_graph_callback](base::OnceClosure quit_closure) {
    on_graph_callback();
    std::move(quit_closure).Run();
  });
}

void RunInGraph(base::FunctionRef<void(Graph*)> on_graph_callback) {
  RunInGraph(
      [&on_graph_callback](base::OnceClosure quit_closure, Graph* graph) {
        on_graph_callback(graph);
        std::move(quit_closure).Run();
      });
}

void RunInGraph(base::FunctionRef<void(GraphImpl*)> on_graph_callback) {
  RunInGraph(
      [&on_graph_callback](base::OnceClosure quit_closure, GraphImpl* graph) {
        on_graph_callback(graph);
        std::move(quit_closure).Run();
      });
}

void RunInGraph(base::FunctionRef<void(base::OnceClosure)> on_graph_callback) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting(
                     [quit_loop = run_loop.QuitClosure(), &on_graph_callback] {
                       on_graph_callback(std::move(quit_loop));
                     }));
  run_loop.Run();
}

void RunInGraph(
    base::FunctionRef<void(base::OnceClosure, Graph*)> on_graph_callback) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([quit_loop = run_loop.QuitClosure(),
                                             &on_graph_callback](Graph* graph) {
        on_graph_callback(std::move(quit_loop), graph);
      }));
  run_loop.Run();
}

void RunInGraph(
    base::FunctionRef<void(base::OnceClosure, GraphImpl*)> on_graph_callback) {
  base::RunLoop run_loop;
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindLambdaForTesting([quit_loop = run_loop.QuitClosure(),
                                  &on_graph_callback](GraphImpl* graph) {
        on_graph_callback(std::move(quit_loop), graph);
      }));
  run_loop.Run();
}

}  // namespace performance_manager
