// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"

class TabGroupEditorBubbleInteractiveUiTest : public InteractiveBrowserTest {
 public:
  TabGroupEditorBubbleInteractiveUiTest() = default;
  ~TabGroupEditorBubbleInteractiveUiTest() override = default;

  static void UpdateGroupIfOpen(TabGroupEditorBubbleView* bubble) {
    // If the widget is already closing, it means it has reacted to the
    // group closure and is safe from further updates.
    if (bubble->GetWidget()->IsClosed()) {
      return;
    }
    // If the widget is NOT closing yet, this will crash because the
    // tab group is gone from the model but the bubble doesn't know yet.
    bubble->UpdateGroup();
  }
};

IN_PROC_BROWSER_TEST_F(TabGroupEditorBubbleInteractiveUiTest,
                       WindowCloseClosesBubble) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowserTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);

  RunTestSequence(
      InstrumentTab(kBrowserTabId, 0),
      NavigateWebContents(kBrowserTabId, GURL("https://google.com")),
      InstrumentNextTab(kNewTabId),
      ExecuteJs(kBrowserTabId,
                "() => { window.popup = window.open('about:blank'); }"),
      WaitForWebContentsReady(kNewTabId),
      WithElement(kNewTabId,
                  [this](ui::TrackedElement* el) {
                    auto* contents =
                        AsInstrumentedWebContents(el)->web_contents();
                    auto* model = browser()->tab_strip_model();
                    int tab_index = model->GetIndexOfWebContents(contents);
                    auto group = model->AddToNewGroup({tab_index});
                    // Bypassing tracker to ensure the fix is what closes the
                    // bubble
                    TabGroupEditorBubbleView::Show(
                        browser(), group,
                        BrowserView::GetBrowserViewForBrowser(browser())
                            ->tab_strip_view(),
                        std::nullopt, false);
                  }),
      // Verify bubble is open.
      WaitForShow(TabGroupEditorBubbleView::kTabGroupEditorBubbleViewId),
      // Trigger the background closure.
      ExecuteJs(kBrowserTabId, "() => { window.popup.close(); }"),

      // Attempt to trigger the crash by updating the bubble after tab closure.
      Do([this]() {
        auto* const bubble =
            views::ElementTrackerViews::GetInstance()
                ->GetFirstMatchingViewAs<TabGroupEditorBubbleView>(
                    TabGroupEditorBubbleView::kTabGroupEditorBubbleViewId,
                    BrowserView::GetBrowserViewForBrowser(browser())
                        ->GetElementContext());

        if (bubble) {
          UpdateGroupIfOpen(bubble);
        }
      }),
      // Verify bubble is closed.
      WaitForHide(TabGroupEditorBubbleView::kTabGroupEditorBubbleViewId));
}
