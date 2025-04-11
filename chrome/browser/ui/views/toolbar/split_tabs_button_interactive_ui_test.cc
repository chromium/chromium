// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

class SplitTabButtonInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  auto OpenSideBySideTab(int tab_index) {
#if !BUILDFLAG(IS_MAC)
    const char kTabToHover[] = "Tab to hover";
#endif

    return Steps(
#if BUILDFLAG(IS_MAC)
        Do([=, this]() {
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindLambdaForTesting([=, this]() {
                TabStrip* const tab_strip =
                    BrowserView::GetBrowserViewForBrowser(browser())
                        ->tabstrip();
                auto* tab = tab_strip->tab_at(tab_index);
                tab->ShowContextMenu(tab->bounds().CenterPoint(),
                                     ui::mojom::MenuSourceType::kMouse);
              }));
        }),
        // Because context menus run inside of a system message pump that cannot
        // process Chrome tasks, the following steps must be executed
        // immediately on the platform.
        WithoutDelay(SelectMenuItem(TabMenuModel::kSplitTabsMenuItem))
#else
        NameDescendantViewByType<Tab>(kTabStripElementId, kTabToHover,
                                      tab_index),
        MoveMouseTo(kTabToHover), ClickMouse(ui_controls::RIGHT),
        WaitForShow(TabMenuModel::kSplitTabsMenuItem),
        SelectMenuItem(TabMenuModel::kSplitTabsMenuItem)
#endif
    );
  }

  auto UpdateSplitTabButtonPinState(bool should_pin) {
    return Do([=, this]() {
      browser()->profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton,
                                                   should_pin);
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kSideBySide};
};

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest, PinSplitTabButton) {
  RunTestSequence(EnsureNotPresent(kToolbarSplitTabsToolbarButtonElementId),
                  UpdateSplitTabButtonPinState(true),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  UpdateSplitTabButtonPinState(false),
                  WaitForHide(kToolbarSplitTabsToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(SplitTabButtonInteractiveTest,
                       UnpinSplitTabWhileActive) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents1Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents2Id);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents3Id);
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  RunTestSequence(InstrumentTab(kWebContents1Id),
                  NavigateWebContents(kWebContents1Id, url1),
                  AddInstrumentedTab(kWebContents2Id, url1),
                  AddInstrumentedTab(kWebContents3Id, url1),
                  UpdateSplitTabButtonPinState(true),
                  WaitForShow(kToolbarSplitTabsToolbarButtonElementId),
                  SelectTab(kTabStripElementId, 0), OpenSideBySideTab(1),
                  EnsurePresent(kToolbarSplitTabsToolbarButtonElementId),
                  UpdateSplitTabButtonPinState(false),
                  EnsurePresent(kToolbarSplitTabsToolbarButtonElementId));
}
