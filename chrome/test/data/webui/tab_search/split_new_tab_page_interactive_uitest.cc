// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/browser/ui/views/test/split_view_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/view_class_properties.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFourthTab);

using MultiContentsViewObserver =
    views::test::PollingViewObserver<bool, MultiContentsView>;
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(MultiContentsViewObserver,
                                    kMultiContentsViewObserver);

const auto getDeepActiveElement = [](std::string property) {
  return "() => {"
         "  let a = document.activeElement;"
         "  while (a && a.shadowRoot && a.shadowRoot.activeElement) {"
         "    a = a.shadowRoot.activeElement;"
         "  }" +
         std::format("  return a.{};", property) + "}";
};
}  // namespace

class SplitNewTabPageUiTest
    : public SplitViewInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    SplitViewInteractiveTestMixin::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  GURL GetTestUrl() { return embedded_test_server()->GetURL("/title1.html"); }
};

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_Focus DISABLED_Focus
#else
#define MAYBE_Focus Focus
#endif
IN_PROC_BROWSER_TEST_F(SplitNewTabPageUiTest, MAYBE_Focus) {
  RunTestSequence(
      // Create four tabs, with tabs 3 and 4 in a split. Tab 4 is the split view
      // new tab page.
      InstrumentTab(kNewTab), AddInstrumentedTab(kSecondTab, GetTestUrl()),
      AddInstrumentedTab(kThirdTab, GetTestUrl()), EnterSplitView(2),
      InstrumentTab(kFourthTab), FocusElement(kFourthTab),

      // Initially there should be no currently focused element.
      CheckJsResult(kFourthTab, getDeepActiveElement("tagName"),
                    ::testing::Eq("BODY")),

      // The first focusable element should be the close button.
      SendKeyPress(kMultiContentsViewElementId, ui::VKEY_TAB),
      CheckJsResult(kFourthTab, getDeepActiveElement("tagName"),
                    ::testing::Eq("CR-ICON-BUTTON")),
      CheckJsResult(kFourthTab, getDeepActiveElement("id"),
                    ::testing::Eq("closeButton")),

      // Advance focus into the list of open tabs. kSecondTab was the most
      // recently focused tab, at index 1
      SendKeyPress(kMultiContentsViewElementId, ui::VKEY_TAB),
      CheckJsResult(kFourthTab, getDeepActiveElement("tagName"),
                    ::testing::Eq("TAB-SEARCH-ITEM")),
      CheckJsResult(kFourthTab, getDeepActiveElement("data.tab.url.url"),
                    ::testing::Eq(GetTestUrl().spec())),
      CheckJsResult(kFourthTab, getDeepActiveElement("data.tab.index"),
                    ::testing::Eq(1)),

      // Advance focus again. kNewTab was the next most recently focused tab, at
      // index 0
      SendKeyPress(kMultiContentsViewElementId, ui::VKEY_TAB),
      CheckJsResult(kFourthTab, getDeepActiveElement("tagName"),
                    ::testing::Eq("TAB-SEARCH-ITEM")),
      CheckJsResult(kFourthTab, getDeepActiveElement("data.tab.url.url"),
                    ::testing::Eq(url::kAboutBlankURL)),
      CheckJsResult(kFourthTab, getDeepActiveElement("data.tab.index"),
                    ::testing::Eq(0)),

      // Advance focus again. Focus should leave the web contents, to the mini
      // toolbar.
      SendKeyPress(kMultiContentsViewElementId, ui::VKEY_TAB),
      CheckJsResult(kFourthTab, getDeepActiveElement("tagName"),
                    ::testing::Eq("BODY")),
      // Focus change doesn't happen synchronously after the tab key press so
      // CheckViewProperty is flaky here - it just so happens that the above
      // CheckJsResult usually takes long enough for it to pass. Use PollView
      // instead for robustness.
      PollView(kMultiContentsViewObserver, kMultiContentsViewElementId,
               [&](const MultiContentsView* multi_contents_view) -> bool {
                 return multi_contents_view->GetActiveContentsContainerView()
                     ->mini_toolbar()
                     ->image_button_for_testing()
                     ->HasFocus();
               }));
}
