// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/test/fullscreen_test_util.h"
#include "chrome/browser/ui/test/popup_test_base.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace {

// Tests popups with multi-screen features from the Window Management API.
// Tests are run with and without the requisite Window Management permission.
// Tests must run in series to manage virtual displays on supported platforms.
// Use 2+ physical displays to run locally with --gtest_also_run_disabled_tests.
// See: //docs/ui/display/multiscreen_testing.md
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_PopupMultiScreenTest PopupMultiScreenTest
#else
#define MAYBE_PopupMultiScreenTest DISABLED_PopupMultiScreenTest
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/371121282): Re-enable the test.
// TODO(crbug.com/365126887): Re-enable the test.
#undef MAYBE_PopupMultiScreenTest
#define MAYBE_PopupMultiScreenTest DISABLED_PopupMultiScreenTest
#endif  // BUILDFLAG(IS_WIN)
class MAYBE_PopupMultiScreenTest : public PopupTestBase,
                                   public ::testing::WithParamInterface<bool> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PopupTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    if (!SetUpVirtualDisplays()) {
      GTEST_SKIP() << "Skipping test; unavailable multi-screen support.";
    }
    ASSERT_GE(display::Screen::GetScreen()->GetNumDisplays(), 2);
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(NavigateToURL(web_contents,
                embedded_test_server()->GetURL("/simple.html")));
    EXPECT_TRUE(WaitForRenderFrameReady(web_contents->GetPrimaryMainFrame()));
    if (ShouldTestWindowManagement()) {
      SetUpWindowManagement(browser());
    }
  }

  void TearDownOnMainThread() override {
    virtual_display_util_.reset();
  }

 protected:
  bool ShouldTestWindowManagement() { return GetParam(); }

  // Create virtual displays as needed, ensuring 2 displays are available for
  // testing multi-screen functionality. Not all platforms and OS versions are
  // supported. Returns false if virtual displays could not be created.
  bool SetUpVirtualDisplays() {
    if (display::Screen::GetScreen()->GetNumDisplays() > 1) {
      return true;
    }
    if ((virtual_display_util_ = display::test::VirtualDisplayUtil::TryCreate(
             display::Screen::GetScreen()))) {
      virtual_display_util_->AddDisplay(
          display::test::VirtualDisplayUtil::k1024x768);
      return true;
    }
    return false;
  }

 private:
  std::unique_ptr<display::test::VirtualDisplayUtil> virtual_display_util_;
};

INSTANTIATE_TEST_SUITE_P(, MAYBE_PopupMultiScreenTest, ::testing::Bool());

// Tests opening a popup without explicit bounds.
IN_PROC_BROWSER_TEST_P(MAYBE_PopupMultiScreenTest, Basic) {
  // Copy the display vector so references are not invalidated while looping.
  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const display::Display& opener_display : displays) {
    browser()->window()->SetBounds(opener_display.work_area());
    ASSERT_EQ(opener_display.id(), GetDisplayNearestBrowser(browser()).id());
    for (const char* url : {"/simple.html", "about:blank"}) {
      const std::string open_script =
          content::JsReplace("open($1, '', 'popup');", url);
      Browser* popup = OpenPopup(browser(), open_script);
      display::Display popup_display = GetDisplayNearestBrowser(popup);
      // The popup should open on the same screen as the opener.
      EXPECT_EQ(opener_display.id(), popup_display.id())
          << " expected: " << opener_display.work_area().ToString()
          << " actual: " << popup_display.work_area().ToString()
          << " popup: " << popup->window()->GetBounds().ToString()
          << " script: " << open_script;
      // The popup is constrained to the available bounds of its screen.
      const gfx::Rect popup_bounds = popup->window()->GetBounds();
      EXPECT_TRUE(popup_display.work_area().Contains(popup_bounds))
          << " work_area: " << popup_display.work_area().ToString()
          << " popup: " << popup_bounds.ToString();
    }
  }
}

// Tests opening a popup on another screen.
IN_PROC_BROWSER_TEST_P(MAYBE_PopupMultiScreenTest, OpenOnAnotherScreen) {
  // Copy the display vector so references are not invalidated while looping.
  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const display::Display& opener_display : displays) {
    browser()->window()->SetBounds(opener_display.work_area());
    ASSERT_EQ(opener_display.id(), GetDisplayNearestBrowser(browser()).id());
    for (const display::Display& target_display : displays) {
      for (const char* url : {"/simple.html", "about:blank"}) {
        const std::string open_script = content::JsReplace(
            "open($1, '', 'left=$2,top=$3,width=200,height=200');", url,
            target_display.work_area().x(), target_display.work_area().y());
        Browser* popup = OpenPopup(browser(), open_script);
        display::Display popup_display = GetDisplayNearestBrowser(popup);
        // The popup only opens on another screen with permission.
        const display::Display& expected_display =
            ShouldTestWindowManagement() ? target_display : opener_display;
        EXPECT_EQ(expected_display.id(), popup_display.id())
            << " expected: " << expected_display.work_area().ToString()
            << " actual: " << popup_display.work_area().ToString()
            << " opener: " << browser()->window()->GetBounds().ToString()
            << " popup: " << popup->window()->GetBounds().ToString()
            << " script: " << open_script;
        // The popup is constrained to the available bounds of its screen.
        const gfx::Rect popup_bounds = popup->window()->GetBounds();
        EXPECT_TRUE(popup_display.work_area().Contains(popup_bounds))
            << " work_area: " << popup_display.work_area().ToString()
            << " popup: " << popup_bounds.ToString();
      }
    }
  }
}

// Tests opening a popup on the same screen, then moving it to another screen.
// TODO(crbug.com/365057654): Test is failing on Mac bot.
IN_PROC_BROWSER_TEST_P(MAYBE_PopupMultiScreenTest,
                       DISABLED_MoveToAnotherScreen) {
  content::WebContents* opener_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Copy the display vector so references are not invalidated while looping.
  display::Screen* screen = display::Screen::GetScreen();
  std::vector<display::Display> displays = screen->GetAllDisplays();
  for (const display::Display& opener_display : displays) {
    browser()->window()->SetBounds(opener_display.work_area());
    ASSERT_EQ(opener_display.id(), GetDisplayNearestBrowser(browser()).id());
    gfx::Point opener_display_center = opener_display.work_area().CenterPoint();
    for (const display::Display& target_display : displays) {
      for (const char* url : {"/simple.html", "about:blank"}) {
        const std::string open_script = content::JsReplace(
            "w = open($1, '', 'left=$2,top=$3,width=200,height=200');", url,
            opener_display_center.x() - 100, opener_display_center.y() - 100);
        Browser* popup = OpenPopup(browser(), open_script);
        EXPECT_EQ(opener_display, GetDisplayNearestBrowser(popup));
        // Ensure the opener can access the popup window object.
        ASSERT_NE("", EvalJs(opener_contents, "w.location.href"));

        // Have the opener try to move the popup to the target screen.
        const std::string move_script = content::JsReplace(
            "w.moveTo($1, $2);", target_display.work_area().x(),
            target_display.work_area().y());
        {
          SCOPED_TRACE(
              testing::Message() << "\n"
              << "script: " << open_script << " " << move_script << "\n"
              << "opener: " << browser()->window()->GetBounds().ToString()
              << " popup: " << popup->window()->GetBounds().ToString());
          content::ExecuteScriptAsync(opener_contents, move_script);
          WaitForBoundsChange(popup, /*move_by=*/40, /*resize_by=*/0);
        }
        const display::Display popup_display = GetDisplayNearestBrowser(popup);

        // The popup only moves to another screen with permission.
        const display::Display& expected_display =
            ShouldTestWindowManagement() ? target_display : opener_display;
        EXPECT_EQ(expected_display.id(), popup_display.id())
            << " expected: " << expected_display.work_area().ToString()
            << " actual: " << popup_display.work_area().ToString()
            << " opener: " << browser()->window()->GetBounds().ToString()
            << " popup: " << popup->window()->GetBounds().ToString()
            << " script: " << open_script << " " << move_script;
        // The popup is constrained to the available bounds of its screen.
        const gfx::Rect popup_bounds = popup->window()->GetBounds();
        EXPECT_TRUE(popup_display.work_area().Contains(popup_bounds))
            << " work_area: " << popup_display.work_area().ToString()
            << " popup: " << popup_bounds.ToString();
      }
    }
  }
}

// Tests opening a popup on another screen from a cross-origin iframe.
IN_PROC_BROWSER_TEST_P(MAYBE_PopupMultiScreenTest, CrossOriginIFrame) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents,
                            https_server.GetURL("a.com", "/simple.html")));
  EXPECT_TRUE(WaitForRenderFrameReady(web_contents->GetPrimaryMainFrame()));
  // Grant permission to the new origin after navigation.
  if (ShouldTestWindowManagement()) {
    SetUpWindowManagement(browser());
  }

  // Append cross-origin iframes with and without the permission policy.
  const GURL src = https_server.GetURL("b.com", "/simple.html");
  const std::string script = R"JS(
    new Promise(resolve => {
      let f = document.createElement('iframe');
      f.src = $1;
      f.allow = $2 ? 'window-management' : '';
      f.addEventListener('load', () => resolve(true));
      document.body.appendChild(f);
    });
  )JS";
  EXPECT_EQ(true, EvalJs(web_contents, content::JsReplace(script, src, false)));
  EXPECT_EQ(true, EvalJs(web_contents, content::JsReplace(script, src, true)));

  // Copy the display vector so references are not invalidated while looping.
  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const display::Display& opener_display : displays) {
    browser()->window()->SetBounds(opener_display.work_area());
    ASSERT_EQ(opener_display.id(), GetDisplayNearestBrowser(browser()).id());
    for (const bool iframe_policy_granted : {true, false}) {
      content::RenderFrameHost* cross_origin_iframe =
          ChildFrameAt(web_contents, iframe_policy_granted ? 1 : 0);
      ASSERT_TRUE(cross_origin_iframe);
      ASSERT_NE(cross_origin_iframe->GetLastCommittedOrigin(),
                web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
      for (const display::Display& target_display : displays) {
        for (const char* url : {"/simple.html", "about:blank"}) {
          const std::string open_script = content::JsReplace(
              "w = open($1, '', 'left=$2,top=$3,width=200,height=200');", url,
              target_display.work_area().x(), target_display.work_area().y());
          Browser* popup = OpenPopup(cross_origin_iframe, open_script);
          display::Display popup_display = GetDisplayNearestBrowser(popup);
          // The popup only opens on another screen with permission.
          const display::Display& expected_display =
              ShouldTestWindowManagement() && iframe_policy_granted
                  ? target_display
                  : opener_display;
          EXPECT_EQ(expected_display.id(), popup_display.id())
              << " expected: " << expected_display.work_area().ToString()
              << " actual: " << popup_display.work_area().ToString()
              << " opener: " << browser()->window()->GetBounds().ToString()
              << " popup: " << popup->window()->GetBounds().ToString()
              << " script: " << open_script;
        }
      }
    }
  }
}

}  // namespace
