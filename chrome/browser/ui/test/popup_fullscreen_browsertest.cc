// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/test/popup_test_base.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {

// Tests opening popups as fullscreen windows.
// See https://chromestatus.com/feature/6002307972464640 for more information.
// Tests are run with and without the requisite Window Management permission.
class PopupFullscreenTest : public PopupTestBase,
                            public ::testing::WithParamInterface<bool> {
 public:
  PopupFullscreenTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kFullscreenPopupWindows}, {});
  }

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(embedded_test_server());
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/empty.html")));
    if (ShouldTestWindowManagement()) {
      SetUpWindowManagement(browser());
    }
  }

 protected:
  bool ShouldTestWindowManagement() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(, PopupFullscreenTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(PopupFullscreenTest, BasicFullscreen) {
  Browser* popup =
      OpenPopup(browser(), "open('/empty.html', '_blank', 'popup,fullscreen')");
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  if (ShouldTestWindowManagement()) {
    WaitForHTMLFullscreen(popup_contents);
  }
  EXPECT_EQ(EvalJs(popup_contents,
                   "!!document.fullscreenElement && "
                   "document.fullscreenElement == document.documentElement")
                .ExtractBool(),
            ShouldTestWindowManagement());
  FullscreenController* fullscreen_controller =
      popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(),
            ShouldTestWindowManagement());
  EXPECT_EQ(EvalJs(popup_contents, "document.exitFullscreen()").error.empty(),
            ShouldTestWindowManagement());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());

  // Test that a navigation doesn't re-trigger fullscreen.
  EXPECT_TRUE(EvalJs(popup_contents,
                     "window.location.href = '" +
                         embedded_test_server()->GetURL("/title1.html").spec() +
                         "'")
                  .error.empty());
  EXPECT_TRUE(content::WaitForLoadStop(popup_contents));
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

IN_PROC_BROWSER_TEST_P(PopupFullscreenTest, AboutBlankFullscreen) {
  Browser* popup =
      OpenPopup(browser(), "open('about:blank', '_blank', 'popup,fullscreen')");
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  if (ShouldTestWindowManagement()) {
    WaitForHTMLFullscreen(popup_contents);
  }
  EXPECT_EQ(EvalJs(popup_contents,
                   "!!document.fullscreenElement && "
                   "document.fullscreenElement == document.documentElement")
                .ExtractBool(),
            ShouldTestWindowManagement());
  FullscreenController* fullscreen_controller =
      popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(),
            ShouldTestWindowManagement());
  EXPECT_EQ(EvalJs(popup_contents, "document.exitFullscreen()").error.empty(),
            ShouldTestWindowManagement());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());

  // Test that a navigation doesn't re-trigger fullscreen.
  EXPECT_TRUE(EvalJs(popup_contents,
                     "window.location.href = '" +
                         embedded_test_server()->GetURL("/title1.html").spec() +
                         "'")
                  .error.empty());
  EXPECT_TRUE(content::WaitForLoadStop(popup_contents));
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

IN_PROC_BROWSER_TEST_P(PopupFullscreenTest, FullscreenWithBounds) {
  Browser* popup =
      OpenPopup(browser(),
                "open('/empty.html', '_blank', "
                "'height=200,width=200,top=100,left=100,fullscreen')");
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  if (ShouldTestWindowManagement()) {
    WaitForHTMLFullscreen(popup_contents);
  }
  EXPECT_EQ(EvalJs(popup_contents,
                   "!!document.fullscreenElement && "
                   "document.fullscreenElement == document.documentElement")
                .ExtractBool(),
            ShouldTestWindowManagement());
  FullscreenController* fullscreen_controller =
      popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(),
            ShouldTestWindowManagement());
}

// Fullscreen should not work if the new window is not specified as a popup.
IN_PROC_BROWSER_TEST_P(PopupFullscreenTest, FullscreenRequiresPopupFeature) {
  // OpenPopup() cannot be used here since it waits for a new browser which
  // would not open in this case.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      EvalJs(web_contents, "open('/empty.html', '_blank', 'fullscreen')")
          .error.empty());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_FALSE(
      EvalJs(web_contents, "!!document.fullscreenElement").ExtractBool());
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

// Tests that the fullscreen flag is ignored if the window.open() does not
// result in a new window.
IN_PROC_BROWSER_TEST_P(PopupFullscreenTest, FullscreenRequiresNewWindow) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe.html")));
  // OpenPopup() cannot be used here since it waits for a new browser which
  // would not open in this case. open() targeting a frame named "test" in
  // "iframe.html" will not create a new window.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      EvalJs(web_contents, "open('/empty.html', 'test', 'popup,fullscreen')")
          .error.empty());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_FALSE(
      EvalJs(web_contents, "!!document.fullscreenElement").ExtractBool());
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

}  // namespace
