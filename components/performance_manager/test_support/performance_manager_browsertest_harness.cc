// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"

#include <memory>

#include "base/run_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "components/performance_manager/embedder/performance_manager_lifetime.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace performance_manager {

PerformanceManagerBrowserTestHarness::PerformanceManagerBrowserTestHarness() =
    default;

PerformanceManagerBrowserTestHarness::~PerformanceManagerBrowserTestHarness() =
    default;

void PerformanceManagerBrowserTestHarness::SetUp() {
  // We use a ConditionVariable instead of RunLoop because the task environment
  // isn't initialized until *after* calling Super::SetUp, but we need to setup
  // the callback before that point.
  base::Lock lock;
  base::ConditionVariable cv(&lock);
  bool graph_initialization_complete = false;
  PerformanceManagerLifetime::SetGraphFeaturesOverrideForTesting(
      GraphFeatures::WithNone());
  PerformanceManagerLifetime::SetAdditionalGraphCreatedCallbackForTesting(
      base::BindLambdaForTesting([&](Graph* graph) {
        OnGraphCreatedImpl(graph);
        base::AutoLock auto_lock(lock);
        graph_initialization_complete = true;
        cv.Signal();
      }));

  // The PM gets initialized in the following, so this must occur after setting
  // up the callback.
  Super::SetUp();

  // Wait until the PM is initialized and callbacks have been invoked on the
  // PM sequence.
  base::AutoLock auto_lock(lock);
  while (!graph_initialization_complete)
    cv.Wait();
}

void PerformanceManagerBrowserTestHarness::PreRunTestOnMainThread() {
  Super::PreRunTestOnMainThread();

  // Set up the embedded web server.
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "components/test/data/performance_manager");
  ASSERT_TRUE(embedded_test_server()->Start());
}

void PerformanceManagerBrowserTestHarness::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Ensure the PM logic is enabled in renderers.
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "PerformanceManagerInstrumentation");
}

void PerformanceManagerBrowserTestHarness::OnGraphCreated(Graph* graph) {}

content::Shell* PerformanceManagerBrowserTestHarness::CreateShell() {
  content::Shell* shell = CreateBrowser();
  return shell;
}

void PerformanceManagerBrowserTestHarness::StartNavigation(
    content::WebContents* contents,
    const GURL& url) {
  // See content/public/test/browser_test_utils.cc
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  contents->GetController().LoadURLWithParams(params);
  contents->Focus();
}

::testing::AssertionResult
PerformanceManagerBrowserTestHarness::NavigateAndWaitForConsoleMessage(
    content::WebContents* contents,
    const GURL& url,
    base::StringPiece console_pattern) {
  content::WebContentsConsoleObserver console_observer(contents);
  console_observer.SetPattern(std::string(console_pattern));
  if (NavigateToURL(contents, url) && console_observer.Wait())
    return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure();
}

namespace {

class WaitForLoadObserver : public content::WebContentsObserver {
 public:
  explicit WaitForLoadObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  ~WaitForLoadObserver() override = default;

  void Wait() {
    if (!web_contents()->IsLoading())
      return;
    run_loop_.Run();
  }

 private:
  // WebContentsObserver implementation
  void DidStopLoading() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;
};

}  // namespace

void PerformanceManagerBrowserTestHarness::WaitForLoad(
    content::WebContents* contents) {
  WaitForLoadObserver observer(contents);
  observer.Wait();
}

void PerformanceManagerBrowserTestHarness::OnGraphCreatedImpl(Graph* graph) {
  graph_features_.ConfigureGraph(graph);
  OnGraphCreated(graph);
}

}  // namespace performance_manager
