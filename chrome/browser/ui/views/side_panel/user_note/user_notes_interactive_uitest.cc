// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "url/gurl.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kUserNotesElementId);

}  // namespace

class UserNotesInteractiveTest : public InteractiveBrowserTest {
 public:
  UserNotesInteractiveTest() = default;
  ~UserNotesInteractiveTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {user_notes::kUserNotes, power_bookmarks::kPowerBookmarkBackend}, {});
    set_open_about_blank_on_browser_launch(true);
    InteractiveBrowserTest::SetUp();
  }

  MultiStep HoverTabAt(int index) {
    const char kTabToHover[] = "Tab to hover";
    return Steps(NameDescendantViewByType<Tab>(kBrowserViewElementId,
                                               kTabToHover, index),
                 MoveMouseTo(kTabToHover));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserNotesInteractiveTest,
                       TriggerNotesSidePanelFromTabContextMenu) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementReadyEvent);
  const DeepQuery kLastUserNoteQuery{"user-notes-app", "user-notes-list",
                                     "user-note:last-of-type"};
  StateChange state_change;
  state_change.event = kElementReadyEvent;
  state_change.type = StateChange::Type::kExists;
  state_change.where = kLastUserNoteQuery;
  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Right click anywhere on the tab to open the context menu.
      HoverTabAt(0), ClickMouse(ui_controls::RIGHT),
      // Make sure context menu is displayed correctly.
      // WaitForShow and FlushEvents is required to prevent re-enter
      // views::MenuController::OpenMenu from the same call stack during the
      // NotifyElementShown.
      InAnyContext(WaitForShow(TabMenuModel::kAddANoteTabMenuItem)),
      FlushEvents(),
      // Click add a note option in the context menu and wait for the side panel
      // to show.
      InAnyContext(SelectMenuItem(TabMenuModel::kAddANoteTabMenuItem)),
      WaitForShow(kUserNotesSidePanelWebViewElementId),
      InstrumentNonTabWebView(kUserNotesElementId,
                              kUserNotesSidePanelWebViewElementId),
      WaitForStateChange(kUserNotesElementId, std::move(state_change)),
      CheckJsResultAt(kUserNotesElementId, kLastUserNoteQuery,
                      "el => el.shadowRoot.activeElement === "
                      "el.shadowRoot.getElementById('noteContent')",
                      true));
}
