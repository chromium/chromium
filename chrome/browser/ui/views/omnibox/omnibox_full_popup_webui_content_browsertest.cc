// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_full_popup_webui_content.h"

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "url/gurl.h"

class OmniboxFullPopupWebUIContentBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxFullPopupWebUIContentBrowserTest() {
    feature_list_.InitWithFeatures({omnibox::internal::kWebUIOmniboxPopup,
                                    omnibox::internal::kWebUIOmniboxFullPopup},
                                   {features::kWebUILocationBar});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return location_bar_view()->IsFullWebUiOmniboxReady(); }));
    browser()->GetTabStripModel()->GetActiveWebContents()->Focus();
    ASSERT_TRUE(content());
  }

 protected:
  LocationBarView* location_bar_view() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetLocationBarView();
  }
  OmniboxPopupViewWebUI* popup_view() {
    return static_cast<OmniboxPopupViewWebUI*>(
        location_bar_view()->GetOmniboxPopupView());
  }
  OmniboxFullPopupWebUIContent* content() {
    return popup_view()->presenter()
               ? static_cast<OmniboxFullPopupWebUIContent*>(
                     popup_view()->presenter()->GetWebUIContent())
               : nullptr;
  }

  void RequestContextMenu() {
    ASSERT_TRUE(popup_view()->RequestWebUIContextMenu(
        location_bar_view()->omnibox_view()->GetBoundsInScreen().CenterPoint(),
        ui::mojom::MenuSourceType::kMouse));
    ASSERT_TRUE(HasPendingContextMenuRequest());
  }

  bool HasPendingContextMenuRequest() {
    return content()->pending_context_menu_request_.has_value();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Focusing the omnibox again supersedes a pending request, so a right-click
// whose menu was never shown can't pop up the menu on a later focus.
IN_PROC_BROWSER_TEST_F(OmniboxFullPopupWebUIContentBrowserTest,
                       PendingRequestDroppedOnFocus) {
  RequestContextMenu();

  location_bar_view()->FocusLocation(/*is_user_initiated=*/true,
                                     /*clear_focus_if_failed=*/false);

  EXPECT_FALSE(HasPendingContextMenuRequest());
}

// Hiding the popup drops a pending request. The presenter has no widget here,
// so this covers `OmniboxPopupFullPresenter::Hide()` cancelling it directly
// rather than via `Clear()`, which the base `Hide()` only reaches with a
// widget.
IN_PROC_BROWSER_TEST_F(OmniboxFullPopupWebUIContentBrowserTest,
                       PendingRequestDroppedOnHide) {
  RequestContextMenu();

  popup_view()->presenter()->Hide();

  EXPECT_FALSE(HasPendingContextMenuRequest());
}

// Clearing the popup content drops a pending request.
IN_PROC_BROWSER_TEST_F(OmniboxFullPopupWebUIContentBrowserTest,
                       PendingRequestDroppedOnClear) {
  RequestContextMenu();

  content()->Clear();

  EXPECT_FALSE(HasPendingContextMenuRequest());
}

// The popup widget losing activation drops a pending request, so it can't be
// shown when the widget is activated again later.
IN_PROC_BROWSER_TEST_F(OmniboxFullPopupWebUIContentBrowserTest,
                       PendingRequestDroppedOnPopupDeactivated) {
  RequestContextMenu();

  content()->OnPopupWidgetActivationChanged(/*active=*/false);

  EXPECT_FALSE(HasPendingContextMenuRequest());
}
