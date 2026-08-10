// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <string_view>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"
#include "url/gurl.h"

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabElementId);
constexpr std::string_view kElementName = "MenuItemElement";

class TrackedElementManagerBrowsertest : public InteractiveBrowserTest {
 public:
  TrackedElementManagerBrowsertest() = default;
  ~TrackedElementManagerBrowsertest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);

    // Get the window and resize it to the maximum width of the current display.
    // This prevents the target page from going into its "collapsed" state where
    // the elements we want are hidden.
    auto* const window = browser()->GetWindow();
    const auto display = display::Screen::Get()->GetDisplayNearestWindow(
        window->GetNativeWindow());
    gfx::Rect bounds = window->GetBounds();
    bounds.set_x(display.work_area().x());
    bounds.set_width(display.work_area().width());
    window->SetBounds(bounds);
  }
};

IN_PROC_BROWSER_TEST_F(TrackedElementManagerBrowsertest, CheckElementsExist) {
  gfx::Rect menu_bounds;
  RunTestSequence(
      InstrumentTab(kTabElementId),
      NavigateWebContents(kTabElementId,
                          GURL(chrome::kChromeUIUserEducationInternalsURL)),
      // Wait for the menu element to appear and capture its bounds.
      InAnyContext(AfterShow(UserEducationInternalsUI::kMenuElementId,
                             [&menu_bounds](ui::TrackedElement* el) {
                               auto* const dom_el =
                                   el->AsA<ui::TrackedElementWebUI>();
                               ASSERT_NE(nullptr, dom_el);
                               menu_bounds = dom_el->GetScreenBounds();
                             })),
      InSameContext(
          // Wait for all menu items to be present.
          WaitForElementCount(UserEducationInternalsUI::kMenuItemElementId, 8U),
          // Grab the menu item for section 2.
          NameElementWithSecondaryId(
              UserEducationInternalsUI::kMenuItemElementId, "index:2",
              kElementName, /*wait_for_present=*/false)),
      // Verify that the menu item is inside the menu.
      CheckElement(kElementName,
                   [&menu_bounds](ui::TrackedElement* el) {
                     const auto item_bounds = el->GetScreenBounds();
                     return !item_bounds.IsEmpty() &&
                            menu_bounds.Contains(item_bounds);
                   }),
      // Click the menu item
      PressButton(kElementName),
      // Verify that the index changes in the document.
      CheckJsResultAt(kTabElementId, {"user-education-internals"},
                      "el => el.selectedTabIndex", 2));
}
