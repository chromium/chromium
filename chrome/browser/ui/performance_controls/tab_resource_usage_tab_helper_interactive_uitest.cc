// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_metrics_refresh_waiter.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

class TabResourceUsageTabHelperTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        performance_manager::features::kMemoryUsageInHovercards);

    InteractiveBrowserTest::SetUp();
  }

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
      MemoryMetricsRefreshWaiter waiter;
      waiter.Wait();
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabResourceUsageTabHelperTest, MemoryUsagePopulated) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents, GetURL()),
      ForceRefreshMemoryMetrics(), Check([=]() {
        content::WebContents* const web_contents =
            browser()->tab_strip_model()->GetWebContentsAt(0);
        auto* const resource_usage =
            TabResourceUsageTabHelper::FromWebContents(web_contents);
        return resource_usage && resource_usage->GetMemoryUsageInBytes() != 0;
      }));
}
