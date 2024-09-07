// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_SAVER_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_SAVER_BROWSER_TEST_MIXIN_H_

#include <concepts>
#include <string_view>
#include <vector>

#include "base/json/values_util.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/performance_controls/test_support/memory_metrics_refresh_waiter.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "net/dns/mock_host_resolver.h"

namespace {
constexpr base::TimeDelta kShortDelay = base::Seconds(1);
}  // namespace

// Template to be used as a mixin class for memory saver tests extending
// InProcessBrowserTest.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class MemorySaverBrowserTestMixin : public T {
 public:
  template <class... Args>
  explicit MemorySaverBrowserTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...),
        scoped_set_clocks_for_testing_(&test_clock_, &test_tick_clock_) {
    // Start with a non-null TimeTicks, as there is no discard protection for
    // a tab with a null focused timestamp.
    test_tick_clock_.Advance(kShortDelay);
  }

  ~MemorySaverBrowserTestMixin() override = default;

  MemorySaverBrowserTestMixin(const MemorySaverBrowserTestMixin&) = delete;
  MemorySaverBrowserTestMixin& operator=(const MemorySaverBrowserTestMixin&) =
      delete;

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();

    // To avoid flakes when focus changes, set the active tab strip model
    // explicitly.
    resource_coordinator::GetTabLifecycleUnitSource()
        ->SetFocusedTabStripModelForTesting(T::browser()->tab_strip_model());

    T::host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(T::embedded_test_server()->Start());
  }

  void SetMemorySaverModeEnabled(bool enabled) {
    performance_manager::user_tuning::UserPerformanceTuningManager::
        GetInstance()
            ->SetMemorySaverModeEnabled(enabled);
  }

  GURL GetURL(std::string_view hostname = "example.com",
              std::string_view path = "/title1.html") {
    return T::embedded_test_server()->GetURL(hostname, path);
  }

  content::WebContents* GetWebContentsAt(int index) {
    return T::browser()->tab_strip_model()->GetWebContentsAt(index);
  }

  bool IsTabDiscarded(int tab_index) {
    return GetWebContentsAt(tab_index)->WasDiscarded();
  }

  bool TryDiscardTabAt(int tab_index) {
    auto* manager = performance_manager::user_tuning::
        UserPerformanceTuningManager::GetInstance();
    manager->DiscardPageForTesting(GetWebContentsAt(tab_index));

    return IsTabDiscarded(tab_index);
  }

  void ForceRefreshMemoryMetricsAndWait() {
    MemoryMetricsRefreshWaiter waiter;
    waiter.Wait();
  }

  void SetTabDiscardExceptionsMap(std::vector<std::string> patterns) {
    base::Value::Dict exclusion_map;
    for (auto pattern : patterns) {
      exclusion_map.Set(pattern, base::TimeToValue(base::Time::Now()));
    }
    T::browser()->profile()->GetPrefs()->SetDict(
        performance_manager::user_tuning::prefs::
            kTabDiscardingExceptionsWithTime,
        std::move(exclusion_map));
  }

 private:
  base::SimpleTestClock test_clock_;
  base::SimpleTestTickClock test_tick_clock_;
  resource_coordinator::ScopedSetClocksForTesting
      scoped_set_clocks_for_testing_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_MEMORY_SAVER_BROWSER_TEST_MIXIN_H_
