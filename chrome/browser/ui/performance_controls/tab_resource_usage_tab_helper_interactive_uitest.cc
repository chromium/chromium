// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
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
#include "ui/base/interaction/element_tracker.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kUpdatedEvent);
constexpr uint64_t kMaxByteUsed = std::numeric_limits<int64_t>::max();
}  // namespace

class TabResourceUsageTabHelperUiTest : public InteractiveBrowserTest {
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

  using WithTabHelperCallback =
      base::OnceCallback<void(TabResourceUsageTabHelper&)>;
  auto WithTabHelper(ui::ElementIdentifier instrumented_tab_id,
                     WithTabHelperCallback callback) {
    return WithElement(
        instrumented_tab_id,
        [callback = std::move(callback)](ui::TrackedElement* el) mutable {
          auto* const tab = tabs::TabInterface::GetFromContents(
              AsInstrumentedWebContents(el)->web_contents());
          CHECK(tab);
          std::move(callback).Run(
              *tab->GetTabFeatures()->resource_usage_helper());
        });
  }

  template <typename T>
  using CheckTabHelperCallback =
      base::OnceCallback<T(TabResourceUsageTabHelper&)>;
  template <typename T, typename M>
  auto CheckTabHelper(ui::ElementIdentifier instrumented_tab_id,
                      CheckTabHelperCallback<T> callback,
                      M&& matcher) {
    return CheckElement(
        instrumented_tab_id,
        [callback = std::move(callback)](ui::TrackedElement* el) mutable -> T {
          auto* const tab = tabs::TabInterface::GetFromContents(
              AsInstrumentedWebContents(el)->web_contents());
          CHECK(tab);
          return std::move(callback).Run(
              *tab->GetTabFeatures()->resource_usage_helper());
        },
        std::forward<M>(matcher));
  }
};

IN_PROC_BROWSER_TEST_F(TabResourceUsageTabHelperUiTest, MemoryUsagePopulated) {
  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GetURL()),
      ForceRefreshMemoryMetrics(),
      CheckTabHelper(kFirstTabContents,
                     base::BindOnce([](TabResourceUsageTabHelper& helper) {
                       return helper.GetMemoryUsageInBytes();
                     }),
                     testing::Ne(0)));
}

IN_PROC_BROWSER_TEST_F(TabResourceUsageTabHelperUiTest,
                       MemoryUsageUpdatesAfterNavigation) {
  std::unique_ptr<ResourceUsageCollectorObserver> observer;
  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      WithTabHelper(kFirstTabContents,
                    base::BindOnce([](TabResourceUsageTabHelper& helper) {
                      helper.SetMemoryUsageInBytes(kMaxByteUsed);
                    })),
      WithElement(
          kBrowserViewElementId,
          [&observer](ui::TrackedElement* el) {
            observer = std::make_unique<ResourceUsageCollectorObserver>(
                base::BindLambdaForTesting([el]() {
                  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
                      el, kUpdatedEvent);
                }));
          }),
      NavigateWebContents(kFirstTabContents, GetURL()),
      WaitForEvent(kBrowserViewElementId, kUpdatedEvent),
      CheckTabHelper(kFirstTabContents,
                     base::BindOnce([](TabResourceUsageTabHelper& helper) {
                       return helper.GetMemoryUsageInBytes();
                     }),
                     testing::Ne(kMaxByteUsed)));
}
