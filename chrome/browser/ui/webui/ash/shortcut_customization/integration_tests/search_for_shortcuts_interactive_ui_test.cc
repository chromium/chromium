// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/shortcut_customization/integration_tests/shortcut_customization_test_base.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

class SearchForShortcutsInteractiveUiTest
    : public ShortcutCustomizationInteractiveUiTestBase {
 public:
  const DeepQuery kSearchBoxQuery{
      "shortcut-customization-app",
      "#searchBoxWrapper > search-box",
      "#search",
      "#searchInput",
  };

  const DeepQuery kNoSearchResultsContainerQuery{
      "shortcut-customization-app",
      "#searchBoxWrapper > search-box",
      "#noSearchResultsContainer",
  };

  const DeepQuery kClearSearchButtonQuery{
      "shortcut-customization-app",
      "#searchBoxWrapper > search-box",
      "#search",
      "#clearSearch",
  };

  const DeepQuery kSearchRowActionQuery{
      "shortcut-customization-app",
      "#searchBoxWrapper > search-box",
      "#frb0",
      "#searchResultRowInner",
  };

  const DeepQuery kActiveNavTabQuery{
      "shortcut-customization-app",
      "#navigationPanel",
      "#navigationSelector > navigation-selector",
      "#navigationSelectorMenu > cr-button.navigation-item.selected",
  };

  // Enters lower-case text into the focused html input element.
  auto EnterLowerCaseText(const std::string& text) {
    return Do([&]() {
      for (char c : text) {
        ui_controls::SendKeyPress(
            /*window=*/nullptr,
            static_cast<ui::KeyboardCode>(ui::VKEY_A + (c - 'a')),
            /*control=*/false, /*shift=*/false,
            /*alt=*/false, /*command=*/false);
      }
    });
  }

  const DeepQuery kRedoActionAcceleratorRowQuery{
      "shortcut-customization-app",
      "navigation-view-panel#navigationPanel",
      "#category-3",
      // Text editing subsection
      "#contentWrapper > accelerator-subsection:nth-child(2)",
      "#rowList > accelerator-row:nth-child(10)",
  };

  auto FocusSearchBox() {
    CHECK(webcontents_id_);
    return Steps(ExecuteJsAt(webcontents_id_, kSearchBoxQuery,
                             "(el) => { el.focus(); el.select(); }"));
  }

  ui::test::InteractiveTestApi::StepBuilder SendKeyPressEvent(
      ui::KeyboardCode key,
      int modifier) {
    return Do([key, modifier]() {
      ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
          .PressKey(key, modifier);
    });
  }
};

IN_PROC_BROWSER_TEST_F(SearchForShortcutsInteractiveUiTest,
                       SearchForShortcuts) {
  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InAnyContext(Steps(
          Log("Focusing search box"), FocusSearchBox(),
          Log("Searching for shortcut 'hxz' which should have no results"),
          EnterLowerCaseText("hxz"),
          Log("Verifying that no shortcuts were found"),
          WaitForElementTextContains(webcontents_id_,
                                     kNoSearchResultsContainerQuery,
                                     "No search results"),
          Log("Clearing search box"),
          ExecuteJsAt(webcontents_id_, kClearSearchButtonQuery, kClickFn),
          Log("Refocusing search box"), FocusSearchBox(),
          Log("Searching for 'Redo last action' shortcut"),
          EnterLowerCaseText("redo"),
          Log("Verifying that 'Redo last action' search result row is visible"),
          WaitForElementExists(webcontents_id_, kSearchRowActionQuery),
          Log("Navigating to 'Redo last action' accelerator"),
          SendKeyPressEvent(ui::VKEY_RETURN, ui::EF_NONE),
          Log("Verifying that 'Text' nav tab is active and 'Redo last action' "
              "accelerator is visible"),
          WaitForElementTextContains(webcontents_id_, kActiveNavTabQuery,
                                     "Text"),
          ExecuteJsAt(webcontents_id_, kRedoActionAcceleratorRowQuery,
                      "el => { return !!el;}"))));
}

}  // namespace
}  // namespace ash
