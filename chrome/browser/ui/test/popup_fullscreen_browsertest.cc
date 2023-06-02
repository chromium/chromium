// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/test/popup_test_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {

// Base class for fullscreen popup tests.
class PopupFullscreenTestBase : public PopupTestBase {
 public:
  PopupFullscreenTestBase() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kFullscreenPopupWindows}, {});
  }

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(embedded_test_server());
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(NavigateToURL(web_contents,
                              embedded_test_server()->GetURL("/simple.html")));
    EXPECT_TRUE(WaitForRenderFrameReady(web_contents->GetPrimaryMainFrame()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests opening popups as fullscreen windows.
// See https://chromestatus.com/feature/6002307972464640 for more information.
// Tests are run with and without the requisite Window Management permission.
class PopupFullscreenTest : public PopupFullscreenTestBase,
                            public ::testing::WithParamInterface<bool> {
 public:
  void SetUpOnMainThread() override {
    PopupFullscreenTestBase::SetUpOnMainThread();
    if (ShouldTestWindowManagement()) {
      SetUpWindowManagement(browser());
    }
  }

 protected:
  bool ShouldTestWindowManagement() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(, PopupFullscreenTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(PopupFullscreenTest, BasicFullscreen) {
  Browser* popup = OpenPopup(
      browser(), "open('/simple.html', '_blank', 'popup,fullscreen')");
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
                "open('/simple.html', '_blank', "
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
      EvalJs(web_contents, "open('/simple.html', '_blank', 'fullscreen')")
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
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents,
                            embedded_test_server()->GetURL("/iframe.html")));
  EXPECT_TRUE(WaitForRenderFrameReady(web_contents->GetPrimaryMainFrame()));
  // OpenPopup() cannot be used here since it waits for a new browser which
  // would not open in this case. open() targeting a frame named "test" in
  // "iframe.html" will not create a new window.
  EXPECT_TRUE(
      EvalJs(web_contents, "open('/simple.html', 'test', 'popup,fullscreen')")
          .error.empty());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_FALSE(
      EvalJs(web_contents, "!!document.fullscreenElement").ExtractBool());
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

struct PopupFullscreenPermissionPolicyTestParams {
  std::string test_name;
  std::string opener_permission_policy_header;
  std::string opened_permission_policy_header;
  bool fullscreen_expected;
};

constexpr char kOpenerPath[] = "/simple.html";
constexpr char kOpenedPath[] = "/title1.html";

std::unique_ptr<net::test_server::HttpResponse> SetPermissionsPolicyHeader(
    std::string opener_header,
    std::string opened_header,
    const net::test_server::HttpRequest& request) {
  const GURL& url = request.GetURL();
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  // The hostname is always 127.0.0.1 here regardless of hostname used in the
  // browser request. The path is used to differentiate between the opener and
  // opened frame.
  if (url.path() == kOpenerPath && !opener_header.empty()) {
    response->AddCustomHeader("Permissions-Policy", opener_header);
  }
  if (url.path() == kOpenedPath && !opened_header.empty()) {
    response->AddCustomHeader("Permissions-Policy", opened_header);
  }
  return response;
}

// Tests fullscreen popup functionality with `fullscreen` permission policy
// being allowed or blocked in the opener (initiator) and/or opened frame.
class PopupFullscreenPermissionPolicyTest
    : public PopupFullscreenTestBase,
      public ::testing::WithParamInterface<
          PopupFullscreenPermissionPolicyTestParams> {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SetPermissionsPolicyHeader, GetParam().opener_permission_policy_header,
        GetParam().opened_permission_policy_header));
    PopupFullscreenTestBase::SetUpOnMainThread();
    SetUpWindowManagement(browser());
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PopupFullscreenPermissionPolicyTest,
    testing::ValuesIn(std::vector<PopupFullscreenPermissionPolicyTestParams>{
        {.test_name = "DefaultOpener_DefaultOpened",
         .opener_permission_policy_header = "",
         .opened_permission_policy_header = "",
         .fullscreen_expected = true},
        {.test_name = "DefaultOpener_SelfOpened",
         .opener_permission_policy_header = "",
         .opened_permission_policy_header = "fullscreen=(self)",
         .fullscreen_expected = true},
        {.test_name = "SelfOpener_DefaultOpened",
         .opener_permission_policy_header = "fullscreen=(self)",
         .opened_permission_policy_header = "",
         .fullscreen_expected = true},
        {.test_name = "SelfOpener_SelfOpened",
         .opener_permission_policy_header = "fullscreen=(self)",
         .opened_permission_policy_header = "fullscreen=(self)",
         .fullscreen_expected = true},
        {.test_name = "BlockedOpener_SelfOpened",
         .opener_permission_policy_header = "fullscreen=()",
         .opened_permission_policy_header = "fullscreen=(self)",
         .fullscreen_expected = false},
        {.test_name = "SelfOpener_BlockedOpened",
         .opener_permission_policy_header = "fullscreen=(self)",
         .opened_permission_policy_header = "fullscreen=()",
         .fullscreen_expected = false},
        {.test_name = "BlockedOpener_BlockedOpened",
         .opener_permission_policy_header = "fullscreen=()",
         .opened_permission_policy_header = "fullscreen=()",
         .fullscreen_expected = false}}),
    [](const testing::TestParamInfo<PopupFullscreenPermissionPolicyTestParams>&
           info) { return info.param.test_name; });

// Opens a fullscreen popup and checks if fullscreen is granted based on the
// expected result for the given permission policy configurations in the test
// parameters.
IN_PROC_BROWSER_TEST_P(PopupFullscreenPermissionPolicyTest,
                       PermissionPolicyTest) {
  std::string url =
      embedded_test_server()->GetURL("cross-origin.com", kOpenedPath).spec();
  Browser* popup =
      OpenPopup(browser(), "open('" + url + "', '_blank', 'popup,fullscreen')");
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  if (GetParam().fullscreen_expected) {
    WaitForHTMLFullscreen(popup_contents);
  }
  EXPECT_EQ(EvalJs(popup_contents,
                   "!!document.fullscreenElement && "
                   "document.fullscreenElement == document.documentElement")
                .ExtractBool(),
            GetParam().fullscreen_expected);
  FullscreenController* fullscreen_controller =
      popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(),
            GetParam().fullscreen_expected);
}

}  // namespace
