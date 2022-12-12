// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "url/gurl.h"

class SidePanelInteractiveTest : public InteractiveBrowserTest {
 public:
  SidePanelInteractiveTest() = default;
  ~SidePanelInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    scoped_feature_list_.InitWithFeatures({features::kUnifiedSidePanel}, {});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    auto* config = SideSearchConfig::Get(browser()->profile());
    config->set_skip_on_template_url_changed_for_testing(true);
  }

  // We can't use SelectDropdownItem directly in the test sequence since
  // SidePanel uses a delayed combobox implementation
  static auto SelectSidePanelEntry(SidePanelEntry::Id id) {
    return base::BindOnce(
        [](int index, ui::TrackedElement* element) {
          auto* const combobox = AsView<views::Combobox>(element);
          combobox->MenuSelectionAt(index);
        },
        static_cast<int>(id));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SidePanelInteractiveTest, ToggleSidePanelVisibility) {
  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click on the toolbar button to open the side panel
      PressButton(kSidePanelButtonElementId), WaitForShow(kSidePanelElementId),
      FlushEvents(),
      // Click on the toolbar button to close the side panel
      PressButton(kSidePanelButtonElementId), WaitForHide(kSidePanelElementId),
      // Click on the toolbar button again open the side panel
      PressButton(kSidePanelButtonElementId), WaitForShow(kSidePanelElementId),
      FlushEvents(),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId));
}

IN_PROC_BROWSER_TEST_F(SidePanelInteractiveTest,
                       SwitchBetweenDifferentEntries) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBookmarksWebContentsId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kReadLaterWebContentsId);

  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click the toolbar button to open the side panel
      PressButton(kSidePanelButtonElementId), WaitForShow(kSidePanelElementId),
      // Switch to the bookmarks entry using the header combobox
      WithElement(kSidePanelComboboxElementId,
                  SelectSidePanelEntry(SidePanelEntry::Id::kBookmarks)),
      InstrumentNonTabWebView(kBookmarksWebContentsId,
                              kBookmarkSidePanelWebViewElementId),
      FlushEvents(),
      // Switch to the reading list entry using the header combobox
      WithElement(kSidePanelComboboxElementId,
                  SelectSidePanelEntry(SidePanelEntry::Id::kReadingList)),
      InstrumentNonTabWebView(kReadLaterWebContentsId,
                              kReadLaterSidePanelWebViewElementId),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId),
      EnsureNotPresent(kReadLaterSidePanelWebViewElementId));
}

IN_PROC_BROWSER_TEST_F(SidePanelInteractiveTest,
                       StaysOpenOnTabSwitchWithActiveGlobalEntry) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);

  RunTestSequence(
      // Add a second tab to the tab strip
      AddInstrumentedTab(kSecondTabElementId, GURL(url::kAboutBlankURL)),
      CheckResult(base::BindLambdaForTesting([this]() {
                    return browser()->tab_strip_model()->active_index();
                  }),
                  testing::Eq(1)),
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click the toolbar button to open the side panel
      PressButton(kSidePanelButtonElementId), WaitForShow(kSidePanelElementId),
      FlushEvents(),
      // Switch to the first tab again with the side panel open
      SelectTab(kTabStripElementId, 0),
      // Ensure the side panel is still visible
      CheckViewProperty(kSidePanelElementId, &views::View::GetVisible, true),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId));
}

IN_PROC_BROWSER_TEST_F(SidePanelInteractiveTest,
                       ReopensToLastActiveGlobalEntry) {
  RunTestSequence(
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click the toolbar button to open the side panel
      PressButton(kSidePanelButtonElementId), WaitForShow(kSidePanelElementId),
      // Switch to the bookmarks entry using the header combobox
      WithElement(kSidePanelComboboxElementId,
                  SelectSidePanelEntry(SidePanelEntry::Id::kBookmarks)),
      WaitForShow(kBookmarkSidePanelWebViewElementId), FlushEvents(),
      // Click on the close button to dismiss the side panel
      PressButton(kSidePanelCloseButtonElementId),
      WaitForHide(kSidePanelElementId), FlushEvents(),
      // Click on the toolbar button again open the side panel
      PressButton(kSidePanelButtonElementId),
      // Verify the bookmarks side panel entry is shown (last seen)
      WaitForShow(kBookmarkSidePanelWebViewElementId),
      EnsureNotPresent(kReadLaterSidePanelWebViewElementId));
}
