// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/v8_context_tracker.h"

#include <memory>

#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/command_line.h"
#include "components/performance_manager/execution_context/execution_context_registry_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"
#include "components/performance_manager/test_support/run_in_graph.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "content/public/test/test_utils.h"

namespace performance_manager {
namespace v8_memory {

struct ContextCounts {
  size_t v8_context_count = 0;
  size_t execution_context_count = 0;
  size_t detached_v8_context_count = 0;
  size_t destroyed_execution_context_count = 0;
};

class V8ContextTrackerTest : public PerformanceManagerBrowserTestHarness {
 public:
  using Super = PerformanceManagerBrowserTestHarness;

  V8ContextTrackerTest() = default;
  ~V8ContextTrackerTest() override = default;

  void SetUp() override {
    GetGraphFeatures().EnableV8ContextTracker();
    Super::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Force site-per-process so the number of internal utility v8 contexts is
    // stable.
    content::IsolateAllSitesForTesting(command_line);
    Super::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    RunInGraph([&](Graph* graph) {
      auto* v8ct = V8ContextTracker::GetFromGraph(graph);
      ASSERT_TRUE(v8ct);

      // The browser could start with execution contexts and/or v8 contexts (for
      // example if it creates a spare renderer loading about:blank, or
      // something preloads a utility context).
      current_counts_.v8_context_count = v8ct->GetV8ContextCountForTesting();
      current_counts_.execution_context_count =
          v8ct->GetExecutionContextCountForTesting();

      // There should not be any detached or destroyed contexts on start.
      EXPECT_EQ(v8ct->GetDetachedV8ContextCountForTesting(), 0u);
      EXPECT_EQ(v8ct->GetDestroyedExecutionContextCountForTesting(), 0u);
    });
    Super::SetUpOnMainThread();
  }

  void ExpectCountIncrease(
      ContextCounts count_change,
      const base::Location& location = base::Location::Current()) {
    RunInGraph([&](Graph* graph) {
      SCOPED_TRACE(location.ToString());
      auto* v8ct = V8ContextTracker::GetFromGraph(graph);
      ASSERT_TRUE(v8ct);

      // There may be extra V8 contexts created, such as for lazily-created
      // utility contexts.
      EXPECT_GE(
          v8ct->GetV8ContextCountForTesting(),
          current_counts_.v8_context_count + count_change.v8_context_count)
          << "expected increase " << count_change.v8_context_count;
      current_counts_.v8_context_count = v8ct->GetV8ContextCountForTesting();

      EXPECT_EQ(v8ct->GetExecutionContextCountForTesting(),
                current_counts_.execution_context_count +
                    count_change.execution_context_count)
          << "expected increase " << count_change.execution_context_count;
      current_counts_.execution_context_count =
          v8ct->GetExecutionContextCountForTesting();

      EXPECT_EQ(v8ct->GetDetachedV8ContextCountForTesting(),
                current_counts_.detached_v8_context_count +
                    count_change.detached_v8_context_count)
          << "expected increase " << count_change.detached_v8_context_count;
      current_counts_.detached_v8_context_count =
          v8ct->GetDetachedV8ContextCountForTesting();

      EXPECT_EQ(v8ct->GetDestroyedExecutionContextCountForTesting(),
                current_counts_.destroyed_execution_context_count +
                    count_change.destroyed_execution_context_count)
          << "expected increase "
          << count_change.destroyed_execution_context_count;
      ;
      current_counts_.destroyed_execution_context_count =
          v8ct->GetDestroyedExecutionContextCountForTesting();
    });
  }

 private:
  ContextCounts current_counts_;
};

// TODO(crbug.com/40931300): Re-enable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_AboutBlank DISABLED_AboutBlank
#else
#define MAYBE_AboutBlank AboutBlank
#endif
IN_PROC_BROWSER_TEST_F(V8ContextTrackerTest, MAYBE_AboutBlank) {
  ASSERT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  ExpectCountIncrease({.v8_context_count = 1, .execution_context_count = 1});
}

IN_PROC_BROWSER_TEST_F(V8ContextTrackerTest, SameOriginIframeAttributionData) {
  GURL urla(embedded_test_server()->GetURL("a.com", "/a_embeds_a.html"));
  auto* contents = shell()->web_contents();
  ASSERT_TRUE(
      NavigateAndWaitForConsoleMessage(contents, urla, "a.html loaded"));

  // Get pointers to the RFHs for each frame.
  content::RenderFrameHost* main_rfh = contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_rfh = ChildFrameAt(main_rfh, 0);
  ASSERT_TRUE(child_rfh);

  auto frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(child_rfh);

  RunInGraph([&frame_node](Graph* graph) {
    ASSERT_TRUE(frame_node);
    auto* v8_context_tracker = V8ContextTracker::GetFromGraph(graph);
    ASSERT_TRUE(v8_context_tracker);
    auto* ec_state = v8_context_tracker->GetExecutionContextState(
        frame_node->GetFrameToken());
    ASSERT_TRUE(ec_state);
    ASSERT_TRUE(ec_state->iframe_attribution_data);
  });
}

// TODO(crbug.com/40931300): Re-enable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CrossOriginIframeAttributionData \
  DISABLED_CrossOriginIframeAttributionData
#else
#define MAYBE_CrossOriginIframeAttributionData CrossOriginIframeAttributionData
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(V8ContextTrackerTest,
                       MAYBE_CrossOriginIframeAttributionData) {
  GURL urla(embedded_test_server()->GetURL("a.com", "/a_embeds_b.html"));
  auto* contents = shell()->web_contents();
  ASSERT_TRUE(
      NavigateAndWaitForConsoleMessage(contents, urla, "b.html loaded"));

  // Get pointers to the RFHs for each frame.
  content::RenderFrameHost* main_rfh = contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_rfh = ChildFrameAt(main_rfh, 0);
  ASSERT_TRUE(child_rfh);
  auto frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(child_rfh);

  RunInGraph([&frame_node](Graph* graph) {
    ASSERT_TRUE(frame_node);
    auto* v8_context_tracker = V8ContextTracker::GetFromGraph(graph);
    ASSERT_TRUE(v8_context_tracker);
    auto* ec_state = v8_context_tracker->GetExecutionContextState(
        frame_node->GetFrameToken());
    ASSERT_TRUE(ec_state);
    ASSERT_TRUE(ec_state->iframe_attribution_data)
        << "url " << frame_node->GetURL() << ", current "
        << frame_node->IsCurrent() << ", state "
        << frame_node->GetLifecycleState();
  });
}

// TODO(crbug.com/40931300): Re-enable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SameSiteNavigation DISABLED_SameSiteNavigation
#else
#define MAYBE_SameSiteNavigation SameSiteNavigation
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(V8ContextTrackerTest, MAYBE_SameSiteNavigation) {
  auto* contents = shell()->web_contents();
  GURL urla(embedded_test_server()->GetURL("a.com", "/a_embeds_b.html"));
  ASSERT_TRUE(
      NavigateAndWaitForConsoleMessage(contents, urla, "b.html loaded"));
  ExpectCountIncrease({.v8_context_count = 2, .execution_context_count = 2});

  // Get pointers to the RFHs for each frame.
  content::RenderFrameHost* rfha = contents->GetPrimaryMainFrame();
  content::RenderFrameHost* rfhb = ChildFrameAt(rfha, 0);
  bool rfh_should_change =
      rfhb->ShouldChangeRenderFrameHostOnSameSiteNavigation();

  // Execute a same site navigation in the child frame. This causes a
  // v8 context to be detached, and new context attached to the execution
  // context.
  GURL urlb(embedded_test_server()->GetURL("b.com", "/b.html?foo=bar"));
  ASSERT_TRUE(ExecJs(
      rfhb, base::StringPrintf("location.href = \"%s\"", urlb.spec().c_str())));
  WaitForLoad(contents);

  if (rfh_should_change) {
    // When RenderDocument is enabled, a new RenderFrameHost will be created for
    // the navigation to `urlb`. Both a new V8 context and ExecutionContext are
    // created, and the old ExecutionContext is destroyed..
    ExpectCountIncrease({.v8_context_count = 1,
                         .execution_context_count = 1,
                         .detached_v8_context_count = 1,
                         .destroyed_execution_context_count = 1});
  } else {
    // When RenderDocument is disabled, the same RenderFrameHost will be reused
    // for the navigation to `urlb`. So only a new V8 context will be created,
    // not a new ExecutionContext.
    ExpectCountIncrease(
        {.v8_context_count = 1, .detached_v8_context_count = 1});
  }
}

IN_PROC_BROWSER_TEST_F(V8ContextTrackerTest, DetachedContext) {
  auto* contents = shell()->web_contents();
  GURL urla(embedded_test_server()->GetURL("a.com", "/a_embeds_a.html"));
  ASSERT_TRUE(
      NavigateAndWaitForConsoleMessage(contents, urla, "a.html loaded"));
  ExpectCountIncrease({.v8_context_count = 2, .execution_context_count = 2});

  // Get pointers to the RFHs for each frame.
  content::RenderFrameHost* rfha = contents->GetPrimaryMainFrame();

  // Keep a pointer to the window associated with the child iframe, but
  // unload it.
  ASSERT_TRUE(ExecJs(rfha,
                     "let iframe = document.getElementsByTagName('iframe')[0]; "
                     "document.body.leakyRef = iframe.contentWindow.window; "
                     "iframe.parentNode.removeChild(iframe); "
                     "console.log('detached and leaked iframe');"));

  ExpectCountIncrease({
      .detached_v8_context_count = 1,
      .destroyed_execution_context_count = 1,
  });
}

}  // namespace v8_memory
}  // namespace performance_manager
