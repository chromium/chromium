// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/policies/prefetch_virtual_memory_policy.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

namespace performance_manager::policies {

PrefetchVirtualMemoryPolicy::PrefetchVirtualMemoryPolicy(
    base::FilePath file_to_prefetch)
    : file_to_prefetch_(std::move(file_to_prefetch)),
      last_prefetch_time_(base::TimeTicks::Now()),
      ongoing_preread_(false),
      weak_ptr_factory_(this) {
  TRACE_EVENT_INSTANT("browser",
                      "PrefetchVirtualMemoryPolicy::"
                      "PrefetchVirtualMemoryPolicy");
}

PrefetchVirtualMemoryPolicy::~PrefetchVirtualMemoryPolicy() {
  TRACE_EVENT_INSTANT("browser",
                      "PrefetchVirtualMemoryPolicy::~"
                      "PrefetchVirtualMemoryPolicy");
}

void PrefetchVirtualMemoryPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddProcessNodeObserver(this);
}

void PrefetchVirtualMemoryPolicy::OnTakenFromGraph(Graph* graph) {
  graph->RemoveProcessNodeObserver(this);
}

// When a process is added to the graph check if we need to refresh the main
// DLL contents in RAM.
void PrefetchVirtualMemoryPolicy::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  if (NeedToRefresh() && !ongoing_preread_) {
    TRACE_EVENT0("browser", "PrefetchVirtualMemoryPolicy::NeedToRefresh");
    ongoing_preread_ = true;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(
            [](base::FilePath file) {
              base::PreReadFile(file,
                                /*is_executable=*/true,
                                /*sequential=*/false);
            },
            file_to_prefetch_),
        base::BindOnce(
            [](base::WeakPtr<PrefetchVirtualMemoryPolicy> policy) {
              if (policy) {
                policy->ongoing_preread_ = false;
              }
            },
            weak_ptr_factory_.GetWeakPtr()));
  }
}

bool PrefetchVirtualMemoryPolicy::NeedToRefresh() {
  // Interval at which we will refresh the main browser DLL.
  constexpr base::TimeDelta kRefreshDelay = base::Seconds(5);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta elapsed = now - last_prefetch_time_;
  if (elapsed > kRefreshDelay) {
    last_prefetch_time_ = now;
    return true;
  }
  return false;
}

}  // namespace performance_manager::policies
