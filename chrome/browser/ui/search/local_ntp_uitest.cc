// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

class LocalNtpUiTest : public InProcessBrowserTest {
 public:
  LocalNtpUiTest() {}

  OmniboxView* omnibox() {
    return browser()->window()->GetLocationBar()->GetOmniboxView();
  }
};

IN_PROC_BROWSER_TEST_F(LocalNtpUiTest, FakeboxRedirectsToOmnibox) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(), 1000);

  // This is required to make the mouse events we send below arrive at the right
  // place. It *should* be the default for all interactive_ui_tests anyway, but
  // on Mac it isn't; see crbug.com/641969.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Make sure that the omnibox doesn't have focus already.
  ui_test_utils::FocusView(browser(), VIEW_ID_TAB_CONTAINER);
  ASSERT_EQ(OMNIBOX_FOCUS_NONE, omnibox()->model()->focus_state());

  // Make sure the fakebox is visible.
  bool result = false;
  const std::string is_fakebox_visible = R"js(
        (function(elem) {
          return elem && elem.offsetWidth > 0 && elem.offsetHeight > 0 &&
              window.getComputedStyle(elem).visibility != 'hidden';
        })(document.getElementById('fakebox')))js";
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(active_tab, is_fakebox_visible,
                                                &result));
  EXPECT_TRUE(result);

  // Get the position of the fakebox on the page.
  double fakebox_x = 0;
  ASSERT_TRUE(instant_test_utils::GetDoubleFromJS(
      active_tab,
      "document.getElementById('fakebox').getBoundingClientRect().left",
      &fakebox_x));
  double fakebox_y = 0;
  ASSERT_TRUE(instant_test_utils::GetDoubleFromJS(
      active_tab,
      "document.getElementById('fakebox').getBoundingClientRect().top",
      &fakebox_y));

  // Move the mouse over the fakebox.
  gfx::Vector2d fakebox_pos(static_cast<int>(std::ceil(fakebox_x)),
                            static_cast<int>(std::ceil(fakebox_y)));
  gfx::Point origin = active_tab->GetContainerBounds().origin();
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(origin + fakebox_pos +
                                               gfx::Vector2d(1, 1)));

  // Click on the fakebox, and wait for the omnibox to receive invisible focus.
  ASSERT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::DOWN));
  ASSERT_TRUE(
      ui_test_utils::SendMouseEventsSync(ui_controls::LEFT, ui_controls::UP));
  ui_test_utils::WaitForViewFocus(browser(), VIEW_ID_OMNIBOX, true);
  EXPECT_EQ(OMNIBOX_FOCUS_INVISIBLE, omnibox()->model()->focus_state());

  // The fakebox should now also have focus.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "document.body.classList.contains('fakebox-focused')",
      &result));
  EXPECT_TRUE(result);

  // Type "a" and wait for the omnibox to receive visible focus.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::KeyboardCode::VKEY_A,
      /*control=*/false, /*shift=*/false, /*alt=*/false, /*command=*/false));
  ui_test_utils::WaitForViewFocus(browser(), VIEW_ID_OMNIBOX, true);
  EXPECT_EQ(OMNIBOX_FOCUS_VISIBLE, omnibox()->model()->focus_state());

  // The typed text should have arrived in the omnibox.
  EXPECT_EQ("a", base::UTF16ToUTF8(omnibox()->GetText()));

  // On the JS side, the fakebox should have been hidden.
  ASSERT_TRUE(instant_test_utils::GetBoolFromJS(active_tab, is_fakebox_visible,
                                                &result));
  EXPECT_FALSE(result);
}
