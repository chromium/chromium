// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/fake_owner.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/events/event_constants.h"

class ContentSettingBubbleModelMixedScriptTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list.InitWithFeatures(
        /* enabled_features */ {},
        /* disabled_features */ {blink::features::kMixedContentAutoupgrade,
                                 features::kMixedContentSiteSetting});
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
  }

  TabSpecificContentSettings* GetActiveTabSpecificContentSettings() {
    return TabSpecificContentSettings::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list;
};

// Tests that a MIXEDSCRIPT type ContentSettingBubbleModel sends appropriate
// IPCs to allow the renderer to load unsafe scripts and refresh the page
// automatically.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMixedScriptTest, MainFrame) {
  GURL url(https_server_->GetURL("/content_setting_bubble/mixed_script.html"));

  // Load a page with mixed content and do quick verification by looking at
  // the title string.
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      ContentSettingsType::MIXEDSCRIPT));

  // Emulate link clicking on the mixed script bubble.
  std::unique_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(),
          browser()->tab_strip_model()->GetActiveWebContents(),
          ContentSettingsType::MIXEDSCRIPT));
  model->OnCustomLinkClicked();

  // Wait for reload
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  observer.Wait();

  EXPECT_FALSE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      ContentSettingsType::MIXEDSCRIPT));
}

class ContentSettingsMixedScriptIgnoreCertErrorsTest
    : public ContentSettingBubbleModelMixedScriptTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentSettingBubbleModelMixedScriptTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }
};

// Tests that a MIXEDSCRIPT type ContentSettingBubbleModel records UMA
// metrics when the content is allowed to run.
IN_PROC_BROWSER_TEST_F(ContentSettingsMixedScriptIgnoreCertErrorsTest,
                       MainFrameMetrics) {
  GURL url(https_server_->GetURL("/content_setting_bubble/mixed_script.html"));

  base::HistogramTester histograms;
  histograms.ExpectTotalCount("ContentSettings.MixedScript", 0);

  // Load a page with mixed content and do quick verification by looking at
  // the title string.
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      ContentSettingsType::MIXEDSCRIPT));

  // Emulate link clicking on the mixed script bubble.
  std::unique_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(),
          browser()->tab_strip_model()->GetActiveWebContents(),
          ContentSettingsType::MIXEDSCRIPT));
  model->OnCustomLinkClicked();

  // Wait for reload
  content::TestNavigationObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  observer.Wait();

  EXPECT_FALSE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      ContentSettingsType::MIXEDSCRIPT));

  // Check that the UMA counts are as expected.
  histograms.ExpectBucketCount(
      "ContentSettings.MixedScript",
      content_settings::MIXED_SCRIPT_ACTION_CLICKED_ALLOW, 1);
}

// Tests that a MIXEDSCRIPT type ContentSettingBubbleModel does not work
// for an iframe (mixed script in iframes is never allowed and the mixed
// content shield isn't shown for it).
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMixedScriptTest, Iframe) {
  GURL url(https_server_->GetURL(
      "/content_setting_bubble/mixed_script_in_iframe.html"));

  ui_test_utils::NavigateToURL(browser(), url);

  // Blink does not ask the browser to handle mixed content in the case
  // of active subresources in an iframe, so the content type should not
  // be marked as blocked.
  EXPECT_FALSE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      ContentSettingsType::MIXEDSCRIPT));
}

class ContentSettingBubbleModelMediaStreamTest : public InProcessBrowserTest {
 public:
  void ManageMediaStreamSettings(
      TabSpecificContentSettings::MicrophoneCameraState state) {
    // Open a tab for which we will invoke the media bubble.
    GURL url(ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("content_setting_bubble"),
        base::FilePath().AppendASCII("mixed_script.html")));
    ui_test_utils::NavigateToURL(browser(), url);
    content::WebContents* original_tab = GetActiveTab();

    // Create a bubble with the given camera and microphone access state.
    TabSpecificContentSettings::FromWebContents(original_tab)->
        OnMediaStreamPermissionSet(
            original_tab->GetLastCommittedURL(),
            state, std::string(), std::string(), std::string(), std::string());
    std::unique_ptr<ContentSettingBubbleModel> bubble(
        new ContentSettingMediaStreamBubbleModel(
            browser()->content_setting_bubble_model_delegate(), original_tab));

    // Click the manage button, which opens in a new tab or window. Wait until
    // it loads.
    bubble->OnManageButtonClicked();
    ASSERT_NE(GetActiveTab(), original_tab);
    content::TestNavigationObserver observer(GetActiveTab());
    observer.Wait();
  }

  content::WebContents* GetActiveTab() {
    // First, we need to find the active browser window. It should be at
    // the same desktop as the browser in which we invoked the bubble.
    Browser* active_browser = chrome::FindLastActive();
    return active_browser->tab_strip_model()->GetActiveWebContents();
  }
};

// Tests that clicking on the manage button in the media bubble opens the
// correct section of the settings UI. This test sometimes leaks memory,
// detected by linux_chromium_asan_rel_ng. See http://crbug/668693 for more
// info.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMediaStreamTest,
                       DISABLED_ManageLink) {
  // For each of the three options, we click the manage button and check if the
  // active tab loads the correct internal url.

  // The microphone bubble links to microphone exceptions.
  ManageMediaStreamSettings(TabSpecificContentSettings::MICROPHONE_ACCESSED);
  EXPECT_EQ(GURL("chrome://settings/contentExceptions#media-stream-mic"),
            GetActiveTab()->GetLastCommittedURL());

  // The bubble for both media devices links to the the first section of the
  // default media content settings, which is the microphone section.
  ManageMediaStreamSettings(TabSpecificContentSettings::MICROPHONE_ACCESSED |
                            TabSpecificContentSettings::CAMERA_ACCESSED);
  EXPECT_EQ(GURL("chrome://settings/content#media-stream-mic"),
            GetActiveTab()->GetLastCommittedURL());

  // The camera bubble links to camera exceptions.
  ManageMediaStreamSettings(TabSpecificContentSettings::CAMERA_ACCESSED);
  EXPECT_EQ(GURL("chrome://settings/contentExceptions#media-stream-camera"),
            GetActiveTab()->GetLastCommittedURL());
}

class ContentSettingBubbleModelPopupTest : public InProcessBrowserTest {
 protected:
  static constexpr int kDisallowButtonIndex = 1;

  void SetUpInProcessBrowserTestFixture() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
  }
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Tests that each popup action is counted in the right bucket.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelPopupTest,
                       PopupsActionsCount){
  GURL url(https_server_->GetURL("/popup_blocker/popup-many.html"));
  base::HistogramTester histograms;
  histograms.ExpectTotalCount("ContentSettings.Popups", 0);

  ui_test_utils::NavigateToURL(browser(), url);

  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_DISPLAYED_BLOCKED_ICON_IN_OMNIBOX, 1);

  // Creates the ContentSettingPopupBubbleModel in order to emulate clicks.
  std::unique_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(),
          browser()->tab_strip_model()->GetActiveWebContents(),
          ContentSettingsType::POPUPS));
  std::unique_ptr<FakeOwner> owner =
      FakeOwner::Create(*model, kDisallowButtonIndex);

  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_DISPLAYED_BUBBLE, 1);

  model->OnListItemClicked(0, ui::EF_LEFT_MOUSE_BUTTON);
  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_CLICKED_LIST_ITEM_CLICKED, 1);

  model->OnManageButtonClicked();
  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_CLICKED_MANAGE_POPUPS_BLOCKING, 1);

  owner->SetSelectedRadioOptionAndCommit(model->kAllowButtonIndex);
  histograms.ExpectBucketCount(
        "ContentSettings.Popups",
        content_settings::POPUPS_ACTION_SELECTED_ALWAYS_ALLOW_POPUPS_FROM, 1);

  histograms.ExpectTotalCount("ContentSettings.Popups", 5);
}

class ContentSettingBubbleModelMixedScriptOopifTest
    : public ContentSettingBubbleModelMixedScriptTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentSettingBubbleModelMixedScriptTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentSettingBubbleModelMixedScriptTest::
        SetUpInProcessBrowserTestFixture();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Tests that a MIXEDSCRIPT type ContentSettingBubbleModel sends appropriate
// IPCs to allow the renderer to load unsafe scripts inside out-of-processs
// iframes.
IN_PROC_BROWSER_TEST_F(ContentSettingBubbleModelMixedScriptOopifTest,
                       MixedContentInCrossSiteIframe) {
  // Create a URL for the mixed content document and append it as a query
  // string to the main URL. This approach is taken because the test servers
  // run on random ports each time and it is not possible to use a static
  // URL in the HTML for the main frame. The main document will use JS to
  // navigate the iframe to the URL specified in the query string, which is
  // determined at runtime and is known to be correct.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/content_setting_bubble/mixed_script.html"));
  GURL main_url(https_server_->GetURL(
      "/content_setting_bubble/mixed_script_in_cross_site_iframe.html?" +
      foo_url.spec()));

  // Load a page with mixed content and verify that mixed content didn't get
  // executed.
  ui_test_utils::NavigateToURL(browser(), main_url);
  EXPECT_TRUE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      ContentSettingsType::MIXEDSCRIPT));

  std::string title;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      ChildFrameAt(web_contents->GetMainFrame(), 0),
      "domAutomationController.send(document.title)", &title));
  EXPECT_EQ("", title);

  // Emulate link clicking on the mixed script bubble.
  content::TestNavigationObserver observer(web_contents);
  std::unique_ptr<ContentSettingBubbleModel> model(
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          browser()->content_setting_bubble_model_delegate(), web_contents,
          ContentSettingsType::MIXEDSCRIPT));
  model->OnCustomLinkClicked();

  // Wait for reload and verify that mixed content is allowed.
  observer.Wait();
  EXPECT_FALSE(GetActiveTabSpecificContentSettings()->IsContentBlocked(
      ContentSettingsType::MIXEDSCRIPT));

  // Ensure that the script actually executed by checking the title of the
  // document in the subframe.
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      ChildFrameAt(web_contents->GetMainFrame(), 0),
      "domAutomationController.send(document.title)", &title));
  EXPECT_EQ("mixed_script_ran_successfully", title);
}
