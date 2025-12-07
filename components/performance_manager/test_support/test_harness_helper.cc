// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/test_harness_helper.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "content/public/browser/browser_context.h"

namespace performance_manager {

PerformanceManagerTestHarnessHelper::PerformanceManagerTestHarnessHelper() =
    default;
PerformanceManagerTestHarnessHelper::~PerformanceManagerTestHarnessHelper() =
    default;

void PerformanceManagerTestHarnessHelper::SetUp() {
  // Allow this to be called multiple times.
  if (perf_man_.get())
    return;
  perf_man_ = PerformanceManagerImpl::Create();
  GraphImpl* graph = PerformanceManagerImpl::GetGraphImpl();
  graph_features_.ConfigureGraph(graph);
  if (graph_impl_callback_) {
    std::move(graph_impl_callback_).Run(graph);
  }
  registry_ = PerformanceManagerRegistry::Create();
}

void PerformanceManagerTestHarnessHelper::TearDown() {
  // Have the performance manager destroy itself.
  registry_->TearDown();
  registry_.reset();

  PerformanceManagerImpl::Destroy(std::move(perf_man_));
}

void PerformanceManagerTestHarnessHelper::OnWebContentsCreated(
    content::WebContents* contents) {
  registry_->CreatePageNodeForWebContents(contents);
}

void PerformanceManagerTestHarnessHelper::OnBrowserContextAdded(
    content::BrowserContext* browser_context) {
  registry_->NotifyBrowserContextAdded(browser_context);
}

void PerformanceManagerTestHarnessHelper::OnBrowserContextRemoved(
    content::BrowserContext* browser_context) {
  registry_->NotifyBrowserContextRemoved(browser_context);
}

GraphFeatures& PerformanceManagerTestHarnessHelper::GetGraphFeatures() {
  // Calling GetGraphFeatures() after the performance manager is initialized
  // would have no effect, so it is restricted here.
  CHECK(!perf_man_);
  return graph_features_;
}

}  // namespace performance_manager
