// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "build/chromeos_buildflags.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/test/fullscreen_test_util.h"
#include "chrome/browser/ui/test/popup_test_base.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {

// Log filter on messages ignoring a fullscreen [popup] request.
bool FullscreenRequestIgnoredMessageFilter(
    const content::WebContentsConsoleObserver::Message& message) {
  return message.message.starts_with(u"Fullscreen request ignored:");
}

// Log filter on messages indicating fullscreen permission policy is denied.
bool FullscreenPermissionPolicyViolationMessageFilter(
    const content::WebContentsConsoleObserver::Message& message) {
  return message.message.starts_with(
      u"Permissions policy violation: fullscreen is not allowed");
}

// Base class for fullscreen popup tests.
class PopupFullscreenTestBase : public PopupTestBase {
 public:
  PopupFullscreenTestBase() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kFullscreenPopupWindows}, {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Required for permission policy violations to be logged.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PermissionsPolicyReporting");
    PopupTestBase::SetUpCommandLine(command_line);
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
    console_observer_ = std::make_unique<content::WebContentsConsoleObserver>(
        browser()->tab_strip_model()->GetActiveWebContents());
    console_observer_->SetFilter(
        base::BindRepeating(&FullscreenRequestIgnoredMessageFilter));
  }

 protected:
  std::unique_ptr<content::WebContentsConsoleObserver> console_observer_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests opening popups as fullscreen windows.
// See https://chromestatus.com/feature/6002307972464640 for more information.
// Tests are run with and without the requisite Window Management permission.
class PopupFullscreenTest
    : public PopupFullscreenTestBase,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUpOnMainThread() override {
    PopupFullscreenTestBase::SetUpOnMainThread();
    if (ShouldTestWindowManagement()) {
      SetUpWindowManagement(browser());
      // The permission prompt creates a user gesture. If we are trying to test
      // without a user gesture, wait for the existing one to expire.
      if (!ShouldTestWithUserGesture()) {
        WaitForUserActivationExpiry(browser());
      }
    }
  }

 protected:
  bool IsFullscreenExpected() {
    return ShouldTestWithUserGesture() && ShouldTestWindowManagement();
  }
  bool ShouldTestWithUserGesture() { return std::get<0>(GetParam()); }
  bool ShouldTestWindowManagement() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(,
                         PopupFullscreenTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

IN_PROC_BROWSER_TEST_P(PopupFullscreenTest, BasicFullscreen) {
  // UMA Key for tracking duration of fullscreen popups.
  static constexpr char kFullscreenDurationMetricKeyPopup[] =
      "Blink.Element.Fullscreen.DurationUpTo1H.Popup";
  base::HistogramTester histogram_tester;
  Browser* popup =
      OpenPopup(browser(), "open('/simple.html', '_blank', 'popup,fullscreen')",
                ShouldTestWithUserGesture());
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  if (IsFullscreenExpected()) {
    content::WaitForHTMLFullscreen(popup_contents);
  } else {
    ASSERT_TRUE(console_observer_->Wait());
  }
  EXPECT_EQ(EvalJs(popup_contents,
                   "!!document.fullscreenElement && "
                   "document.fullscreenElement == document.documentElement")
                .ExtractBool(),
            IsFullscreenExpected());
  FullscreenController* fullscreen_controller =
      popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(), IsFullscreenExpected());
  // Expect no UMA samples logged yet for the popups fullscreen duration.
  histogram_tester.ExpectTotalCount(kFullscreenDurationMetricKeyPopup, 0);
  EXPECT_EQ(EvalJs(popup_contents, "document.exitFullscreen()").error.empty(),
            IsFullscreenExpected());
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
  // Expect exactly 1 UMA sample logged if fullscreen was entered & exited.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount(kFullscreenDurationMetricKeyPopup,
                                    IsFullscreenExpected() ? 1 : 0);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_AboutBlankFullscreen DISABLED_AboutBlankFullscreen
#else
#define MAYBE_AboutBlankFullscreen AboutBlankFullscreen
#endif
IN_PROC_BROWSER_TEST_P(PopupFullscreenTest, MAYBE_AboutBlankFullscreen) {
  Browser* popup =
      OpenPopup(browser(), "open('about:blank', '_blank', 'popup,fullscreen')",
                ShouldTestWithUserGesture());
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  if (IsFullscreenExpected()) {
    content::WaitForHTMLFullscreen(popup_contents);
  } else {
    ASSERT_TRUE(console_observer_->Wait());
  }
  EXPECT_EQ(EvalJs(popup_contents,
                   "!!document.fullscreenElement && "
                   "document.fullscreenElement == document.documentElement")
                .ExtractBool(),
            IsFullscreenExpected());
  FullscreenController* fullscreen_controller =
      popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(), IsFullscreenExpected());
  EXPECT_EQ(EvalJs(popup_contents, "document.exitFullscreen()").error.empty(),
            IsFullscreenExpected());
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
                "'height=200,width=200,top=100,left=100,fullscreen')",
                ShouldTestWithUserGesture());
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  if (IsFullscreenExpected()) {
    content::WaitForHTMLFullscreen(popup_contents);
  } else {
    ASSERT_TRUE(console_observer_->Wait());
  }
  EXPECT_EQ(EvalJs(popup_contents,
                   "!!document.fullscreenElement && "
                   "document.fullscreenElement == document.documentElement")
                .ExtractBool(),
            IsFullscreenExpected());
  FullscreenController* fullscreen_controller =
      popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(), IsFullscreenExpected());
}

// Tests that a fullscreen popup consumes a user gesture even with "popups &
// redirects" enabled and therefore a second fullscreen popup without a gesture
// will not transition to fullscreen.
IN_PROC_BROWSER_TEST_P(PopupFullscreenTest, ConsumesGesture) {
  if (!IsFullscreenExpected()) {
    // This test is only applicable when testing the normal use case.
    GTEST_SKIP();
  }
  // Enable Popups & Redirects.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(web_contents->GetURL(), GURL(),
                                      ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_ALLOW);

  // Open a fullscreen popup with a gesture and validate.
  Browser* popup = OpenPopup(
      browser(), "open('/simple.html', '_blank', 'popup,fullscreen')");
  content::WebContents* popup_contents =
      popup->tab_strip_model()->GetActiveWebContents();
  content::WaitForHTMLFullscreen(popup_contents);
  EXPECT_TRUE(EvalJs(popup_contents,
                     "!!document.fullscreenElement && "
                     "document.fullscreenElement == document.documentElement")
                  .ExtractBool());

  // Open another fullscreen popup without a gesture. The popup should open but
  // the fullscreen should be ignored since the user activation was consumed.
  popup =
      OpenPopup(browser(), "open('/simple.html', '_blank', 'popup,fullscreen')",
                /*user_gesture=*/false);
  popup_contents = popup->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(console_observer_->Wait());
  EXPECT_FALSE(EvalJs(popup_contents,
                      "!!document.fullscreenElement && "
                      "document.fullscreenElement == document.documentElement")
                   .ExtractBool());
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
  EXPECT_EQ(console_observer_->messages().size(), 1u);
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
  ASSERT_TRUE(NavigateToURL(web_contents, embedded_test_server()->GetURL(
                                              "/iframe_about_blank.html")));
  EXPECT_TRUE(WaitForRenderFrameReady(web_contents->GetPrimaryMainFrame()));
  // OpenPopup() cannot be used here since it waits for a new browser which
  // would not open in this case. open() targeting a frame named "test" in
  // "iframe.html" will not create a new window.
  EXPECT_TRUE(
      EvalJs(web_contents,
             "open('/simple.html', 'about_blank_iframe', 'popup,fullscreen')")
          .error.empty());
  EXPECT_EQ(console_observer_->messages().size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_FALSE(
      EvalJs(web_contents, "!!document.fullscreenElement").ExtractBool());
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());
}

struct PopupFullscreenPermissionPolicyTestParams {
  enum ExpectedFullscreenState { Allowed, BlockedByOpener, BlockedByOpened };
  std::string test_name;
  std::string opener_permission_policy_header;
  std::string opened_permission_policy_header;
  ExpectedFullscreenState fullscreen_expected;
  bool is_fullscreen_expected_allowed() const {
    return fullscreen_expected == ExpectedFullscreenState::Allowed;
  }
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

using ExpectedState =
    PopupFullscreenPermissionPolicyTestParams::ExpectedFullscreenState;

INSTANTIATE_TEST_SUITE_P(
    ,
    PopupFullscreenPermissionPolicyTest,
    testing::ValuesIn(std::vector<PopupFullscreenPermissionPolicyTestParams>{
        {.test_name = "DefaultOpener_DefaultOpened",
         .opener_permission_policy_header = "",
         .opened_permission_policy_header = "",
         .fullscreen_expected = ExpectedState::Allowed},
        {.test_name = "DefaultOpener_SelfOpened",
         .opener_permission_policy_header = "",
         .opened_permission_policy_header = "fullscreen=(self)",
         .fullscreen_expected = ExpectedState::Allowed},
        {.test_name = "SelfOpener_DefaultOpened",
         .opener_permission_policy_header = "fullscreen=(self)",
         .opened_permission_policy_header = "",
         .fullscreen_expected = ExpectedState::Allowed},
        {.test_name = "SelfOpener_SelfOpened",
         .opener_permission_policy_header = "fullscreen=(self)",
         .opened_permission_policy_header = "fullscreen=(self)",
         .fullscreen_expected = ExpectedState::Allowed},
        {.test_name = "BlockedOpener_SelfOpened",
         .opener_permission_policy_header = "fullscreen=()",
         .opened_permission_policy_header = "fullscreen=(self)",
         .fullscreen_expected = ExpectedState::BlockedByOpener},
        {.test_name = "SelfOpener_BlockedOpened",
         .opener_permission_policy_header = "fullscreen=(self)",
         .opened_permission_policy_header = "fullscreen=()",
         .fullscreen_expected = ExpectedState::BlockedByOpened},
        {.test_name = "BlockedOpener_BlockedOpened",
         .opener_permission_policy_header = "fullscreen=()",
         .opened_permission_policy_header = "fullscreen=()",
         .fullscreen_expected = ExpectedState::BlockedByOpener}}),
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
  if (GetParam().fullscreen_expected == ExpectedState::BlockedByOpened) {
    // In cases where fullscreen is blocked by the *opened* frame:
    // Switch the console observer to the opened window.
    console_observer_ =
        std::make_unique<content::WebContentsConsoleObserver>(popup_contents);
    console_observer_->SetFilter(
        base::BindRepeating(&FullscreenPermissionPolicyViolationMessageFilter));
  }
  if (GetParam().is_fullscreen_expected_allowed()) {
    content::WaitForHTMLFullscreen(popup_contents);
  } else {
    ASSERT_TRUE(console_observer_->Wait());
  }
  EXPECT_EQ(EvalJs(popup_contents,
                   "!!document.fullscreenElement && "
                   "document.fullscreenElement == document.documentElement")
                .ExtractBool(),
            GetParam().is_fullscreen_expected_allowed());
  FullscreenController* fullscreen_controller =
      popup->exclusive_access_manager()->fullscreen_controller();
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_EQ(fullscreen_controller->IsTabFullscreen(),
            GetParam().is_fullscreen_expected_allowed());
}

}  // namespace
