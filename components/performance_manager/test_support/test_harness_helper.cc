// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/test_harness_helper.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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
  base::RunLoop run_loop;
  GraphImplCallback callback =
      base::BindLambdaForTesting([&](GraphImpl* graph) {
        graph_features_.ConfigureGraph(graph);
        if (graph_impl_callback_)
          std::move(graph_impl_callback_).Run(graph);
        run_loop.Quit();
      });
  perf_man_ = PerformanceManagerImpl::Create(std::move(callback));
  registry_ = PerformanceManagerRegistry::Create();
  run_loop.Run();
}

void PerformanceManagerTestHarnessHelper::TearDown() {
  // Have the performance manager destroy itself.
  registry_->TearDown();
  registry_.reset();

  base::RunLoop run_loop;
  PerformanceManagerImpl::SetOnDestroyedCallbackForTesting(
      run_loop.QuitClosure());
  PerformanceManagerImpl::Destroy(std::move(perf_man_));
  run_loop.Run();
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

}  // namespace performance_manager
