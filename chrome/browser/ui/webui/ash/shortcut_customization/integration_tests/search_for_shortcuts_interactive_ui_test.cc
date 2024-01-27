// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/string_escape.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/shortcut_customization/shortcut_customization_test_base.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

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

  auto SendKeyPressWithoutModifier(ui::KeyboardCode code) {
    return Do(base::BindLambdaForTesting([=]() {
      ASSERT_TRUE(
          ui_test_utils::SendKeyPressSync(browser(), code,
                                          /*control=*/false, /*shift=*/false,
                                          /*alt=*/false, /*command=*/false));
    }));
  }

  const DeepQuery kRedoActionAcceleratorRowQuery{
      "shortcut-customization-app",
      "navigation-view-panel#navigationPanel",
      "#category-3",
      // Text editing subsection
      "#container > accelerator-subsection:nth-child(2)",
      "#rowList > accelerator-row:nth-child(10)",
  };

  auto FocusSearchBox() {
    CHECK(webcontents_id_);
    return Steps(ExecuteJsAt(webcontents_id_, kSearchBoxQuery,
                             "(el) => { el.focus(); el.select(); }"));
  }

  auto WaitForElementTextContains(const DeepQuery& query,
                                  const std::string& text) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTextFound);
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = query;
    state_change.test_function = "function(el) { return el.innerText.indexOf(" +
                                 base::GetQuotedJSONString(text) + ") >= 0; }";
    state_change.event = kTextFound;
    return WaitForStateChange(webcontents_id_, state_change);
  }

  auto WaitForElementExists(const DeepQuery& query) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementExists);
    StateChange element_exists;
    element_exists.event = kElementExists;
    element_exists.where = query;
    return WaitForStateChange(webcontents_id_, element_exists);
  }
};

IN_PROC_BROWSER_TEST_F(SearchForShortcutsInteractiveUiTest,
                       SearchForShortcuts) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShortcutAppWebContentsId);
  webcontents_id_ = kShortcutAppWebContentsId;

  RunTestSequence(
      InstrumentNextTab(kShortcutAppWebContentsId, AnyBrowser()),
      LaunchShortcutCustomizationApp(),
      WaitForWebContentsReady(kShortcutAppWebContentsId,
                              GURL("chrome://shortcut-customization")),
      InAnyContext(Steps(
          Log("Focusing search box"), FocusSearchBox(),
          Log("Searching for shortcut 'hxz' which should have no results"),
          EnterLowerCaseText("hxz"),
          Log("Verifying that no shortcuts were found"),
          WaitForElementTextContains(kNoSearchResultsContainerQuery,
                                     "No search results"),
          Log("Clearing search box"),
          ExecuteJsAt(kShortcutAppWebContentsId, kClearSearchButtonQuery,
                      kClickFn),
          Log("Refocusing search box"), FocusSearchBox(),
          Log("Searching for 'Redo last action' shortcut"),
          EnterLowerCaseText("redo"),
          Log("Verifying that 'Redo last action' search result row is visible"),
          WaitForElementExists(kSearchRowActionQuery),
          Log("Navigating to 'Redo last action' accelerator"),
          SendKeyPressWithoutModifier(ui::VKEY_RETURN),
          Log("Verifying that 'Text' nav tab is active and 'Redo last action' "
              "accelerator is visible"),
          WaitForElementTextContains(kActiveNavTabQuery, "Text"),
          ExecuteJsAt(kShortcutAppWebContentsId, kRedoActionAcceleratorRowQuery,
                      "el => { return !!el;}"))));
}

}  // namespace
}  // namespace ash
