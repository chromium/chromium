// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_metrics_refresh_waiter.h"
#include "chrome/browser/ui/performance_controls/test_support/resource_usage_collector_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

class TabResourceUsageTabHelperTest : public InteractiveBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetURL() {
    return embedded_test_server()->GetURL("example.com", "/title1.html");
  }

  auto ForceRefreshMemoryMetrics() {
    return Do([]() {
      TabResourceUsageRefreshWaiter waiter;
      waiter.Wait();
    });
  }
};

IN_PROC_BROWSER_TEST_F(TabResourceUsageTabHelperTest, MemoryUsagePopulated) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      ForceRefreshMemoryMetrics(), Check([=, this]() {
        content::WebContents* const web_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        auto* const resource_usage =
            TabResourceUsageTabHelper::FromWebContents(web_contents);
        return resource_usage && resource_usage->GetMemoryUsageInBytes() != 0;
      }));
}

IN_PROC_BROWSER_TEST_F(TabResourceUsageTabHelperTest,
                       MemoryUsageUpdatesAfterNavigation) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  auto* const resource_usage =
      TabResourceUsageTabHelper::FromWebContents(web_contents);
  const uint64_t bytes_used = std::numeric_limits<uint64_t>::max();
  resource_usage->SetMemoryUsageInBytes(bytes_used);
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  ResourceUsageCollectorObserver observer(run_loop.QuitClosure());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetURL(), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);
  run_loop.Run();
  EXPECT_NE(bytes_used, resource_usage->GetMemoryUsageInBytes());
}
