// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/subprocess_metrics_provider.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"

namespace metrics {

namespace {

// Returns a random histogram in Allocator. It hard to know which metric will
// not be deprecated and exist after navigation. this will work because we
// always pick up a histogram from render process and verify if it is copied to
// the global allocator.
std::string GetAnyHistogramInAllocator(
    base::PersistentHistogramAllocator* allocator) {
  base::PersistentHistogramAllocator::Iterator iter(allocator);
  std::string histogram_name;
  if (std::unique_ptr<base::HistogramBase> histogram = iter.GetNext())
    histogram_name = histogram->histogram_name();
  return histogram_name;
}

bool HasHistogramInAllocator(base::PersistentHistogramAllocator* allocator,
                             const std::string& histogram_name) {
  base::PersistentHistogramAllocator::Iterator iter(allocator);
  while (std::unique_ptr<base::HistogramBase> histogram = iter.GetNext()) {
    if (histogram_name == histogram->histogram_name())
      return true;
  }
  return false;
}

size_t GetRenderProcessHostCount() {
  auto it = content::RenderProcessHost::AllHostsIterator();
  size_t count = 0;
  while (!it.IsAtEnd()) {
    it.Advance();
    count++;
  }
  return count;
}

}  // namespace

class SubprocessMetricsProviderBrowserTest
    : public content::ContentBrowserTest {
 public:
  void CreateSubprocessMetricsProvider() {
    provider_ = std::make_unique<SubprocessMetricsProvider>();
  }

  SubprocessMetricsProvider::AllocatorByIdMap& get_allocators_by_id() {
    return provider_->allocators_by_id_;
  }

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>&
  get_scoped_observations() {
    return provider_->scoped_observations_;
  }

  base::PersistentHistogramAllocator* GetMainFrameAllocator() {
    return get_allocators_by_id().Lookup(
        shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID());
  }

  void SimulateRenderProcessExit() {
    provider_->RenderProcessExited(
        shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
        content::ChildProcessTerminationInfo());
  }

  void SimulateRenderProcessHostDestroyed() {
    provider_->RenderProcessHostDestroyed(
        shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  }

 protected:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    base::GlobalHistogramAllocator::CreateWithLocalMemory(4 << 20, 0x935DDD43,
                                                          "BrowserMetrics");
    content::ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    content::ContentBrowserTest::TearDown();
    provider_.reset();
  }

 private:
  std::unique_ptr<SubprocessMetricsProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(SubprocessMetricsProviderBrowserTest,
                       RegisterExistingNotReadyRenderProcesses) {
  ASSERT_TRUE(GetRenderProcessHostCount() > 0);
  CreateSubprocessMetricsProvider();
  EXPECT_EQ(get_scoped_observations().GetSourcesCount(),
            GetRenderProcessHostCount());
  EXPECT_EQ(get_allocators_by_id().size(), 0u);

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  content::NavigateToURLBlockUntilNavigationsComplete(shell(), url_a, 1);

  // Verify that the number of scoped observer matches the number of
  // RenderProcessHost and the main frame allocator exists.
  EXPECT_EQ(get_scoped_observations().GetSourcesCount(),
            GetRenderProcessHostCount());
  auto* main_frame_allocator = GetMainFrameAllocator();
  EXPECT_TRUE(main_frame_allocator);

  // Verify the render process's allocator have the render process metrics.
  std::string render_process_histogram =
      GetAnyHistogramInAllocator(main_frame_allocator);
  EXPECT_FALSE(render_process_histogram.empty());

  // Verify the global histogram allocator have no render process metrics.
  base::GlobalHistogramAllocator* global_histogram_allocator =
      base::GlobalHistogramAllocator::Get();
  ASSERT_TRUE(global_histogram_allocator);
  EXPECT_FALSE(HasHistogramInAllocator(global_histogram_allocator,
                                       render_process_histogram))
      << " The histogram in the context is " << render_process_histogram;

  SimulateRenderProcessExit();

  // Verify the allocator deregistered.
  EXPECT_FALSE(GetMainFrameAllocator());

  // Verify the render process metrics were merged to the global histogram
  // allocator.
  EXPECT_TRUE(HasHistogramInAllocator(global_histogram_allocator,
                                      render_process_histogram))
      << " The histogram in the context is " << render_process_histogram;

  auto* main_frame_process_host =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  SimulateRenderProcessHostDestroyed();
  // Verify the observer removed.
  EXPECT_FALSE(
      get_scoped_observations().IsObservingSource(main_frame_process_host));
}

IN_PROC_BROWSER_TEST_F(SubprocessMetricsProviderBrowserTest,
                       RegisterExistingReadyRenderProcesses) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));

  content::NavigateToURLBlockUntilNavigationsComplete(shell(), url_a, 1);

  CreateSubprocessMetricsProvider();

  // Verify that the number of scoped observer matches the number of
  // RenderProcessHost and the main frame allocator exists.
  EXPECT_EQ(get_scoped_observations().GetSourcesCount(),
            GetRenderProcessHostCount());
  auto* main_frame_allocator = GetMainFrameAllocator();
  EXPECT_TRUE(main_frame_allocator);

  // Verify the render process's allocator have the render process metrics.
  std::string render_process_histogram =
      GetAnyHistogramInAllocator(main_frame_allocator);
  EXPECT_FALSE(render_process_histogram.empty());

  // Verify the global histogram allocator have no render process metrics.
  base::GlobalHistogramAllocator* global_histogram_allocator =
      base::GlobalHistogramAllocator::Get();
  ASSERT_TRUE(global_histogram_allocator);
  EXPECT_FALSE(HasHistogramInAllocator(global_histogram_allocator,
                                       render_process_histogram))
      << " The histogram in the context is " << render_process_histogram;

  SimulateRenderProcessExit();

  // Verify the allocator deregistered.
  EXPECT_FALSE(GetMainFrameAllocator());

  // Verify the render process metrics were merged to the global histogram
  // allocator.
  EXPECT_TRUE(HasHistogramInAllocator(global_histogram_allocator,
                                      render_process_histogram))
      << " The histogram in the context is " << render_process_histogram;

  auto* main_frame_process_host =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  SimulateRenderProcessHostDestroyed();
  // Verify the observer removed.
  EXPECT_FALSE(
      get_scoped_observations().IsObservingSource(main_frame_process_host));
}

}  // namespace metrics
