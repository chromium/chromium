// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/test/tab_strip_interactive_test_mixin.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);

constexpr char kDocumentWithTitle[] = "/title3.html";

}  // namespace

class TabStripInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  ~TabStripInteractiveUiTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(TabStripInteractiveUiTest, HoverEffectShowsOnMouseOver) {
  using Observer = views::test::PollingViewObserver<bool, TabStrip>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(Observer, kTabStripHoverState);
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents,
                          embedded_test_server()->GetURL(kDocumentWithTitle)),
      AddInstrumentedTab(kSecondTabContents,
                         embedded_test_server()->GetURL(kDocumentWithTitle)),
      HoverTabAt(0), FinishTabstripAnimations(),
      PollView(kTabStripHoverState, kTabStripElementId,
               [](const TabStrip* tab_strip) -> bool {
                 return tab_strip->tab_at(0)
                            ->tab_style_views()
                            ->GetHoverAnimationValue() > 0;
               }),
      WaitForState(kTabStripHoverState, true));
}
