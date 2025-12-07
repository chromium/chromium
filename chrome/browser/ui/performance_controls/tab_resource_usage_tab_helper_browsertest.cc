// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"

#include "base/byte_count.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_metrics_refresh_waiter.h"
#include "chrome/browser/ui/performance_controls/test_support/resource_usage_collector_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "url/gurl.h"

namespace {
constexpr char kTestDomain[] = "https://foo.bar";
constexpr base::ByteCount kTestMemoryUsage = base::ByteCount(100000);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kUpdatedEvent);
}  // namespace

class TabResourceUsageTabHelperBrowsertest : public InteractiveBrowserTest {
 public:
  TabResourceUsageTabHelperBrowsertest() = default;
  ~TabResourceUsageTabHelperBrowsertest() override = default;

 protected:
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
          std::move(callback).Run(*TabResourceUsageTabHelper::From(tab));
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
          return std::move(callback).Run(*TabResourceUsageTabHelper::From(tab));
        },
        std::forward<M>(matcher));
  }
};

IN_PROC_BROWSER_TEST_F(TabResourceUsageTabHelperBrowsertest,
                       MemoryUsagePopulated) {
  RunTestSequence(
      InstrumentTab(kTabId), NavigateWebContents(kTabId, GetURL()),
      ForceRefreshMemoryMetrics(),
      CheckTabHelper(kTabId,
                     base::BindOnce([](TabResourceUsageTabHelper& helper) {
                       return helper.GetMemoryUsage();
                     }),
                     testing::Ne(base::ByteCount(0))));
}

IN_PROC_BROWSER_TEST_F(TabResourceUsageTabHelperBrowsertest,
                       MemoryUsageUpdatesAfterNavigation) {
  std::unique_ptr<ResourceUsageCollectorObserver> observer;
  RunTestSequence(
      InstrumentTab(kTabId),
      WithTabHelper(kTabId,
                    base::BindOnce([](TabResourceUsageTabHelper& helper) {
                      helper.SetMemoryUsage(base::ByteCount::Max());
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
      NavigateWebContents(kTabId, GetURL()),
      WaitForEvent(kBrowserViewElementId, kUpdatedEvent),
      CheckTabHelper(kTabId,
                     base::BindOnce([](TabResourceUsageTabHelper& helper) {
                       return helper.GetMemoryUsage();
                     }),
                     testing::Ne(base::ByteCount::Max())));
}

// Clears memory usage on navigate.
IN_PROC_BROWSER_TEST_F(TabResourceUsageTabHelperBrowsertest,
                       ClearsMemoryUsageOnNavigate) {
  RunTestSequence(
      InstrumentTab(kTabId),
      WithTabHelper(kTabId,
                    base::BindOnce([](TabResourceUsageTabHelper& tab_helper) {
                      tab_helper.SetMemoryUsage(kTestMemoryUsage);
                    })),
      NavigateWebContents(kTabId, GURL(kTestDomain)),
      CheckTabHelper(kTabId,
                     base::BindOnce([](TabResourceUsageTabHelper& tab_helper) {
                       return tab_helper.GetMemoryUsage();
                     }),
                     base::ByteCount(0)));
}
