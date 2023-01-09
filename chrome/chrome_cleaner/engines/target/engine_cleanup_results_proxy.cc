// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_cleanup_results_proxy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace chrome_cleaner {

EngineCleanupResultsProxy::EngineCleanupResultsProxy(
    mojo::PendingAssociatedRemote<mojom::EngineCleanupResults> cleanup_results,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {
  cleanup_results_.Bind(std::move(cleanup_results));
}

void EngineCleanupResultsProxy::UnbindCleanupResults() {
  cleanup_results_.reset();
}

void EngineCleanupResultsProxy::CleanupDone(uint32_t result) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EngineCleanupResultsProxy::OnDone, this, result));
}

EngineCleanupResultsProxy::EngineCleanupResultsProxy() = default;

EngineCleanupResultsProxy::~EngineCleanupResultsProxy() = default;

void EngineCleanupResultsProxy::OnDone(uint32_t result) {
  if (!cleanup_results_.is_bound()) {
    LOG(ERROR) << "Cleanup result reported after the engine was shut down";
    return;
  }
  cleanup_results_->Done(result);
}

}  // namespace chrome_cleaner
