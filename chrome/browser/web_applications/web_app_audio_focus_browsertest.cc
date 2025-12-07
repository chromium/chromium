// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/media_session/public/cpp/features.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"

namespace web_app {

namespace {

const char kAudioFocusTestPageURL[] =
    "/extensions/audio_focus_web_app/main.html";

}  // namespace

// WebAppAudioFocusBrowserTest test that PWAs have separate audio
// focus from the rest of the browser.
class WebAppAudioFocusBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppAudioFocusBrowserTest() = default;
  ~WebAppAudioFocusBrowserTest() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> features;
#if !BUILDFLAG(IS_CHROMEOS)
    features = apps::test::GetFeaturesToEnableLinkCapturingUX(
        apps::test::LinkCapturingFeatureVersion::kV2DefaultOn);
#endif
    features.emplace_back(media_session::features::kMediaSessionService,
                          base::FieldTrialParams());
    features.emplace_back(media_session::features::kAudioFocusEnforcement,
                          base::FieldTrialParams());
    features.emplace_back(media_session::features::kAudioFocusSessionGrouping,
                          base::FieldTrialParams());

    scoped_feature_list_.InitWithFeaturesAndParameters(
        features, /*disabled_features=*/{});

    WebAppBrowserTestBase::SetUp();
  }

  bool IsPaused(content::WebContents* web_contents) {
    return content::EvalJs(web_contents, "isPaused()").ExtractBool();
  }

  bool WaitForPause(content::WebContents* web_contents) {
    return content::EvalJs(web_contents, "waitForPause()").ExtractBool();
  }

  bool StartPlaying(content::WebContents* web_contents) {
    return content::EvalJs(web_contents, "startPlaying()").ExtractBool();
  }

  content::WebContents* AddTestPageTabAtIndex(int index) {
    EXPECT_TRUE(AddTabAtIndex(
        index, embedded_test_server()->GetURL(kAudioFocusTestPageURL),
        ui::PAGE_TRANSITION_TYPED));
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(tab));
    return tab;
  }

  const base::UnguessableToken& GetAudioFocusGroupId(
      content::WebContents* web_contents) {
    WebAppTabHelper* helper = WebAppTabHelper::FromWebContents(web_contents);
    return helper->GetAudioFocusGroupIdForTesting();
  }

  // Simulates a page calling window.open on an URL and waits for the
  // navigation.
  content::WebContents* OpenWindow(content::WebContents* contents,
                                   bool aux,
                                   const GURL& url) {
    content::WebContentsAddedObserver tab_added_observer;
    EXPECT_TRUE(
        content::ExecJs(contents, "window.open('" + url.spec() + "', '', '" +
                                      (aux ? "opener" : "noopener") + "')"));
    content::WebContents* new_contents = tab_added_observer.GetWebContents();
    EXPECT_TRUE(new_contents);
    WaitForLoadStop(new_contents);

    EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
    EXPECT_EQ(
        content::PAGE_TYPE_NORMAL,
        new_contents->GetController().GetLastCommittedEntry()->GetPageType());
    if (aux) {
      EXPECT_EQ(contents->GetPrimaryMainFrame()->GetSiteInstance(),
                new_contents->GetPrimaryMainFrame()->GetSiteInstance());
    }
    return new_contents;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppAudioFocusBrowserTest, AppHasDifferentAudioFocus) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url = embedded_test_server()->GetURL(kAudioFocusTestPageURL);

  webapps::AppId app_id = InstallPWA(app_url);

  // Launch browser with media page.
  content::WebContents* tab1 = AddTestPageTabAtIndex(0);

  // Start the test page playing.
  EXPECT_TRUE(StartPlaying(tab1));

  // Launch a second tab in the browser.
  content::WebContents* tab2 = AddTestPageTabAtIndex(0);

  // Start the test page playing and check that both tabs now have focus.
  EXPECT_TRUE(StartPlaying(tab2));
  EXPECT_FALSE(IsPaused(tab1));

  // Check that the two tabs have no group id.
  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusGroupId(tab1));
  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusGroupId(tab2));

  // Launch the PWA.
  Browser* app_browser = LaunchWebAppBrowserAndWait(app_id);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Start the PWA playing and check that it has a group id.
  EXPECT_TRUE(StartPlaying(web_contents));
  const base::UnguessableToken& group_id = GetAudioFocusGroupId(web_contents);
  EXPECT_NE(base::UnguessableToken::Null(), group_id);

  // Check that the hosted app took audio focus from the browser tabs.
  EXPECT_TRUE(WaitForPause(tab1));
  EXPECT_TRUE(WaitForPause(tab2));

// TODO(https://crbug.com/376922620): Enable capturing v2 ChromeOS support.
#if !BUILDFLAG(IS_CHROMEOS)
  // Open captured window, which should share the same group id.
  {
    content::WebContents* new_contents =
        OpenWindow(web_contents, /*aux=*/false, app_url);
    EXPECT_EQ(group_id, GetAudioFocusGroupId(new_contents));
  }
  // Open an auxiliary window, which should also open in an app window and share
  // the group id.
  {
    content::WebContents* new_contents =
        OpenWindow(web_contents, /*aux=*/true, app_url);
    EXPECT_EQ(group_id, GetAudioFocusGroupId(new_contents));
  }

  ASSERT_EQ(apps::test::DisableLinkCapturingByUser(profile(), app_id),
            base::ok());
#endif

  // Without capturing, new window will open in the browser so it should have no
  // group id.
  {
    content::WebContents* new_contents =
        OpenWindow(web_contents, /*aux=*/false, app_url);
    EXPECT_EQ(base::UnguessableToken::Null(),
              GetAudioFocusGroupId(new_contents));
  }

  // Navigate inside the PWA and make sure we keep the same group id.
  {
    GURL::Replacements replacements;
    replacements.SetQueryStr("t=1");
    GURL new_url =
        web_contents->GetLastCommittedURL().ReplaceComponents(replacements);
    ASSERT_TRUE(NavigateInRenderer(web_contents, new_url));
    EXPECT_EQ(group_id, GetAudioFocusGroupId(web_contents));
  }

  // Launch a second window for the PWA. It should have the same group id.
  {
    Browser* second_app_browser = LaunchWebAppBrowserAndWait(app_id);

    content::WebContents* new_contents =
        second_app_browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(new_contents));

    EXPECT_EQ(group_id, GetAudioFocusGroupId(new_contents));
  }

  // Navigate away and check that the group id is still the same because we are
  // part of the same window.
  // TODO(crbug.com/40180004): Understand why this returns false.
  ASSERT_FALSE(
      NavigateInRenderer(web_contents, GURL("https://www.example.com")));
  EXPECT_EQ(group_id, GetAudioFocusGroupId(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebAppAudioFocusBrowserTest, WebAppHasSameAudioFocus) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url = embedded_test_server()->GetURL(kAudioFocusTestPageURL);

  webapps::AppId app_id = InstallPWA(app_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusGroupId(web_contents));
}

}  // namespace web_app
