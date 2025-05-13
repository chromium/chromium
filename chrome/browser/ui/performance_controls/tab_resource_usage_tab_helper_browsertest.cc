// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"

#include "base/functional/callback_forward.h"
#include "base/test/bind.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

namespace {
constexpr char kTestDomain[] = "https://foo.bar";
constexpr uint64_t kTestMemoryUsageBytes = 100000;
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
}  // namespace

class TabResourceUsageTabHelperUiTest : public InteractiveBrowserTest {
 protected:
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

// Clears memory usage on navigate.
IN_PROC_BROWSER_TEST_F(TabResourceUsageTabHelperUiTest,
                       ClearsMemoryUsageOnNavigate) {
  RunTestSequence(
      InstrumentTab(kTabId),
      WithTabHelper(kTabId,
                    base::BindOnce([](TabResourceUsageTabHelper& tab_helper) {
                      tab_helper.SetMemoryUsageInBytes(kTestMemoryUsageBytes);
                    })),
      NavigateWebContents(kTabId, GURL(kTestDomain)),
      CheckTabHelper(kTabId,
                     base::BindOnce([](TabResourceUsageTabHelper& tab_helper) {
                       return tab_helper.GetMemoryUsageInBytes();
                     }),
                     0u));
}
