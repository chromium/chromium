// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/cpp/features.h"

namespace web_app {

namespace {

const char kAudioFocusTestPageURL[] =
    "/extensions/audio_focus_web_app/main.html";

}  // namespace

// WebAppAudioFocusBrowserTest test that PWAs have separate audio
// focus from the rest of the browser.
class WebAppAudioFocusBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  WebAppAudioFocusBrowserTest() = default;
  ~WebAppAudioFocusBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media_session::features::kMediaSessionService,
         media_session::features::kAudioFocusEnforcement,
         media_session::features::kAudioFocusSessionGrouping},
        {});

    extensions::ExtensionBrowserTest::SetUp();
  }

  bool IsPaused(content::WebContents* web_contents) {
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, "isPaused()",
                                                     &result));
    return result;
  }

  bool WaitForPause(content::WebContents* web_contents) {
    bool result = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents, "waitForPause()", &result));
    return result;
  }

  bool StartPlaying(content::WebContents* web_contents) {
    bool result = false;
    return content::ExecuteScriptAndExtractBool(web_contents, "startPlaying()",
                                                &result) &&
           result;
  }

  content::WebContents* AddTestPageTabAtIndex(int index) {
    AddTabAtIndex(index, embedded_test_server()->GetURL(kAudioFocusTestPageURL),
                  ui::PAGE_TRANSITION_TYPED);
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(tab));
    return tab;
  }

  const base::UnguessableToken& GetAudioFocusGroupId(
      content::WebContents* web_contents) {
    WebAppTabHelper* helper = WebAppTabHelper::FromWebContents(web_contents);
    return helper->audio_focus_group_id_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(WebAppAudioFocusBrowserTest);
};

IN_PROC_BROWSER_TEST_F(WebAppAudioFocusBrowserTest, AppHasDifferentAudioFocus) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url = embedded_test_server()->GetURL(kAudioFocusTestPageURL);

  // Install the PWA.
  WebApplicationInfo web_app_info;
  web_app_info.app_url = app_url;
  web_app_info.scope = app_url.GetWithoutFilename();
  web_app_info.open_as_window = true;
  const extensions::Extension* extension =
      extensions::browsertest_util::InstallBookmarkApp(browser()->profile(),
                                                       std::move(web_app_info));
  ASSERT_TRUE(extension);

  // Check that the extension has the correct type.
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_TRUE(extension->from_bookmark());

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
  content::WebContents* web_contents;
  {
    ui_test_utils::UrlLoadObserver url_observer(
        app_url, content::NotificationService::AllSources());
    Browser* app_browser = extensions::browsertest_util::LaunchAppBrowser(
        browser()->profile(), extension);
    url_observer.Wait();

    web_contents = app_browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  }

  // Start the PWA playing and check that it has a group id.
  EXPECT_TRUE(StartPlaying(web_contents));
  const base::UnguessableToken& group_id = GetAudioFocusGroupId(web_contents);
  EXPECT_NE(base::UnguessableToken::Null(), group_id);

  // Check that the hosted app took audio focus from the browser tabs.
  EXPECT_TRUE(WaitForPause(tab1));
  EXPECT_TRUE(WaitForPause(tab2));

  // Open a new window from the PWA. It will open in the browser so it should
  // have no group id.
  {
    content::WebContents* new_contents;
    OpenWindow(web_contents, app_url, true, true, &new_contents);
    EXPECT_EQ(base::UnguessableToken::Null(),
              GetAudioFocusGroupId(new_contents));
  }

  // Navigate inside the PWA and make sure we keep the same group id.
  {
    std::string new_query_string = "t=1";
    url::Component new_query(0, new_query_string.length());
    url::Replacements<char> replacements;
    replacements.SetQuery(new_query_string.c_str(), new_query);
    GURL new_url =
        web_contents->GetLastCommittedURL().ReplaceComponents(replacements);
    NavigateInRenderer(web_contents, new_url);
    EXPECT_EQ(group_id, GetAudioFocusGroupId(web_contents));
  }

  // Launch a second window for the PWA. It should have the same group id.
  {
    ui_test_utils::UrlLoadObserver url_observer(
        app_url, content::NotificationService::AllSources());
    Browser* app_browser = extensions::browsertest_util::LaunchAppBrowser(
        browser()->profile(), extension);
    url_observer.Wait();

    content::WebContents* new_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(new_contents));

    EXPECT_EQ(group_id, GetAudioFocusGroupId(new_contents));
  }

  // Clone the web contents and make sure it has a different group id since it
  // is not in an app window.
  {
    std::unique_ptr<content::WebContents> new_contents = web_contents->Clone();
    EXPECT_TRUE(content::WaitForLoadStop(new_contents.get()));
    EXPECT_EQ(base::UnguessableToken::Null(),
              GetAudioFocusGroupId(new_contents.get()));
  }

  // Navigate away and check that the group id is still the same because we are
  // part of the same window.
  NavigateInRenderer(web_contents, GURL("https://www.example.com"));
  EXPECT_EQ(group_id, GetAudioFocusGroupId(web_contents));
}

IN_PROC_BROWSER_TEST_F(WebAppAudioFocusBrowserTest,
                       BookmarkAppHasSameAudioFocus) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url = embedded_test_server()->GetURL(kAudioFocusTestPageURL);

  // Install the bookmark app.
  WebApplicationInfo web_app_info;
  web_app_info.app_url = app_url;
  web_app_info.scope = app_url.GetWithoutFilename();
  web_app_info.open_as_window = true;
  const extensions::Extension* extension =
      extensions::browsertest_util::InstallBookmarkApp(profile(),
                                                       std::move(web_app_info));
  ASSERT_TRUE(extension);

  // Check that the extension has the correct type.
  EXPECT_TRUE(extension->is_hosted_app());
  EXPECT_TRUE(extension->from_bookmark());

  ui_test_utils::NavigateToURL(browser(), app_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusGroupId(web_contents));
}

}  // namespace web_app
