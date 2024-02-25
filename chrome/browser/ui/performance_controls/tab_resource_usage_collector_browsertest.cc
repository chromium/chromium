// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/performance_controls/test_support/resource_usage_collector_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class TabResourceUsageCollectorBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Adds a new tab and waits for that tab to finish receiving memory usage
  // to prevent test from flaking
  void AddAndWaitForTabReady() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    ResourceUsageCollectorObserver observer(run_loop.QuitClosure());
    ASSERT_TRUE(AddTabAtIndex(
        0, embedded_test_server()->GetURL("example.com", "/title1.html"),
        ui::PAGE_TRANSITION_TYPED));
    run_loop.Run();
  }

  TabStripModel* GetTabStripModel() { return browser()->tab_strip_model(); }
};

IN_PROC_BROWSER_TEST_F(TabResourceUsageCollectorBrowserTest,
                       RefreshAllTabMemory) {
  AddAndWaitForTabReady();
  AddAndWaitForTabReady();
  TabStripModel* const model = GetTabStripModel();
  uint64_t bytes_used = 100;
  TabResourceUsageTabHelper* const first_tab_helper =
      TabResourceUsageTabHelper::FromWebContents(model->GetWebContentsAt(0));
  first_tab_helper->SetMemoryUsageInBytes(bytes_used);
  TabResourceUsageTabHelper* const second_tab_helper =
      TabResourceUsageTabHelper::FromWebContents(model->GetWebContentsAt(0));
  second_tab_helper->SetMemoryUsageInBytes(bytes_used);

  // Collector refresh memory usage data for all tabs
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  ResourceUsageCollectorObserver observer(run_loop.QuitClosure());
  TabResourceUsageCollector::Get()->ImmediatelyRefreshMetricsForAllTabs();
  run_loop.Run();

  EXPECT_NE(bytes_used, first_tab_helper->GetMemoryUsageInBytes());
  EXPECT_NE(bytes_used, second_tab_helper->GetMemoryUsageInBytes());
}

IN_PROC_BROWSER_TEST_F(TabResourceUsageCollectorBrowserTest,
                       RefreshMemoryForOneWebContents) {
  AddAndWaitForTabReady();
  AddAndWaitForTabReady();
  TabStripModel* const model = GetTabStripModel();
  uint64_t bytes_used = 100;
  content::WebContents* const first_tab_contents = model->GetWebContentsAt(0);
  TabResourceUsageTabHelper* const first_tab_helper =
      TabResourceUsageTabHelper::FromWebContents(first_tab_contents);
  first_tab_helper->SetMemoryUsageInBytes(bytes_used);
  TabResourceUsageTabHelper* const second_tab_helper =
      TabResourceUsageTabHelper::FromWebContents(model->GetWebContentsAt(1));
  second_tab_helper->SetMemoryUsageInBytes(bytes_used);

  // Collector refresh memory usage data for the first web contents
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  ResourceUsageCollectorObserver observer(run_loop.QuitClosure());
  TabResourceUsageCollector::Get()->ImmediatelyRefreshMetrics(
      first_tab_contents);
  run_loop.Run();

  EXPECT_NE(bytes_used, first_tab_helper->GetMemoryUsageInBytes());
  EXPECT_EQ(bytes_used, second_tab_helper->GetMemoryUsageInBytes());
}
