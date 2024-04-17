// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/tracing.h"

#include "base/allocator/buildflags.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::trace_event::MemoryDumpDeterminism;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpType;
using tracing::BeginTracingWithTraceConfig;
using tracing::EndTracing;

void RequestGlobalDumpCallback(base::OnceClosure quit_closure,
                               bool success,
                               uint64_t) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(quit_closure));
  // TODO(ssid): Check for dump success once crbug.com/709524 is fixed.
}

void OnStartTracingDoneCallback(
    base::trace_event::MemoryDumpLevelOfDetail explicit_dump_type,
    base::OnceClosure quit_closure) {
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDumpAndAppendToTrace(
          MemoryDumpType::kExplicitlyTriggered, explicit_dump_type,
          MemoryDumpDeterminism::kNone,
          BindOnce(&RequestGlobalDumpCallback, std::move(quit_closure)));
}

class MemoryTracingBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    should_test_memory_dump_success_ = false;
    InProcessBrowserTest::SetUp();
  }

  // Execute some no-op javascript on the current tab - this triggers a trace
  // event in RenderFrameImpl::OnJavaScriptExecuteRequestForTests (from the
  // renderer process).
  void ExecuteJavascriptOnCurrentTab() {
    content::WebContents* wc =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(wc);
    ASSERT_TRUE(content::ExecJs(wc, ";"));
  }

  void PerformDumpMemoryTestActions(
      const base::trace_event::TraceConfig& trace_config,
      base::trace_event::MemoryDumpLevelOfDetail explicit_dump_type,
      std::string* json_events) {
    GURL url1("about:blank");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());

    // Begin tracing and trigger dump once start is broadcasted to all
    // processes.
    base::RunLoop run_loop;
    ASSERT_TRUE(BeginTracingWithTraceConfig(
        trace_config, BindOnce(&OnStartTracingDoneCallback, explicit_dump_type,
                               run_loop.QuitClosure())));

    // Create and destroy renderers while tracing is enabled.
    GURL url2("chrome://credits");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());

    // Close the current tab.
    browser()->tab_strip_model()->CloseSelectedTabs();

    GURL url3("chrome://chrome-urls");
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url3, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ASSERT_NO_FATAL_FAILURE(ExecuteJavascriptOnCurrentTab());

    run_loop.Run();
    ASSERT_TRUE(EndTracing(json_events));

    if (should_test_memory_dump_success_) {
      // Expect the basic memory dumps to be present in the trace.
      EXPECT_NE(std::string::npos, json_events->find("process_totals"));
      EXPECT_NE(std::string::npos, json_events->find("v8"));
      EXPECT_NE(std::string::npos, json_events->find("blink_gc"));
    }
  }

  bool should_test_memory_dump_success_;
};

// TODO(crbug.com/41367720): Disabled due to excessive output on lsan bots and
// timeouts on debug bots.
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
#define MAYBE_TestMemoryInfra DISABLED_TestMemoryInfra
#else
#define MAYBE_TestMemoryInfra TestMemoryInfra
#endif
IN_PROC_BROWSER_TEST_F(MemoryTracingBrowserTest, MAYBE_TestMemoryInfra) {
  // TODO(ssid): Test for dump success once the on start tracing done callback
  // is fixed to be called after enable tracing is acked by all processes,
  // crbug.com/709524. The test still tests if dumping does not crash.
  should_test_memory_dump_success_ = false;
  std::string json_events;
  PerformDumpMemoryTestActions(
      base::trace_event::TraceConfig(
          base::trace_event::TraceConfigMemoryTestUtil::
              GetTraceConfig_EmptyTriggers()),
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed, &json_events);
}

// crbug.com/808152: This test is flakily failing on LSAN. This test also
// flakily fails with timeout on Linux debug.
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
#define MAYBE_TestBackgroundMemoryInfra DISABLED_TestBackgroundMemoryInfra
#else
#define MAYBE_TestBackgroundMemoryInfra TestBackgroundMemoryInfra
#endif
IN_PROC_BROWSER_TEST_F(MemoryTracingBrowserTest,
                       MAYBE_TestBackgroundMemoryInfra) {
  // TODO(ssid): Test for dump success once the on start tracing done callback
  // is fixed to be called after enable tracing is acked by all processes,
  // crbug.com/709524. The test still tests if dumping does not crash.
  should_test_memory_dump_success_ = false;
  std::string json_events;
  PerformDumpMemoryTestActions(
      base::trace_event::TraceConfig(
          base::trace_event::TraceConfigMemoryTestUtil::
              GetTraceConfig_BackgroundTrigger(200)),
      base::trace_event::MemoryDumpLevelOfDetail::kBackground, &json_events);
}

}  // namespace
