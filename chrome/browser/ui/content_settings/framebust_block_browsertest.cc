// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/fake_owner.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/blocked_content/url_list_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#endif

namespace {

const int kAllowRadioButtonIndex = 0;
const int kDisallowRadioButtonIndex = 1;

}  // namespace

class FramebustBlockBrowserTest
    : public InProcessBrowserTest,
      public blocked_content::UrlListManager::Observer {
 public:
  FramebustBlockBrowserTest() = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    current_browser_ = InProcessBrowserTest::browser();
    FramebustBlockTabHelper::FromWebContents(GetWebContents())
        ->manager()
        ->AddObserver(this);
  }

  // UrlListManager::Observer:
  void BlockedUrlAdded(int32_t id, const GURL& blocked_url) override {
    if (!blocked_url_added_closure_.is_null())
      std::move(blocked_url_added_closure_).Run();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  FramebustBlockTabHelper* GetFramebustTabHelper() {
    return FramebustBlockTabHelper::FromWebContents(GetWebContents());
  }

  void OnClick(const GURL& url, size_t index, size_t total_size) {
    clicked_url_ = url;
    clicked_index_ = index;
  }

  Browser* browser() { return current_browser_; }

  void CreateAndSetBrowser() {
    current_browser_ = CreateBrowser(browser()->profile());
  }

  bool NavigateIframeToUrlWithoutGesture(content::WebContents* contents,
                                         const std::string iframe_id,
                                         const GURL& url) {
    const char kScript[] = R"(
        var iframe = document.getElementById('%s');
        iframe.src='%s'
    )";
    content::TestNavigationObserver load_observer(contents);
    bool result = content::ExecuteScriptWithoutUserGesture(
        contents,
        base::StringPrintf(kScript, iframe_id.c_str(), url.spec().c_str()));
    load_observer.Wait();
    return result;
  }

 protected:
  base::Optional<GURL> clicked_url_;
  base::Optional<size_t> clicked_index_;

  base::OnceClosure blocked_url_added_closure_;
  Browser* current_browser_;
};

// Tests that clicking an item in the list of blocked URLs trigger a navigation
// to that URL.
IN_PROC_BROWSER_TEST_F(FramebustBlockBrowserTest, ModelAllowsRedirection) {
  const GURL blocked_urls[] = {
      GURL(chrome::kChromeUIHistoryURL), GURL(chrome::kChromeUISettingsURL),
      GURL(chrome::kChromeUIVersionURL),
  };

  // Signal that a blocked redirection happened.
  auto* helper = GetFramebustTabHelper();
  for (const GURL& url : blocked_urls) {
    helper->AddBlockedUrl(url,
                          base::BindOnce(&FramebustBlockBrowserTest::OnClick,
                                         base::Unretained(this)));
  }
  EXPECT_TRUE(helper->HasBlockedUrls());

  // Simulate clicking on the second blocked URL.
  ContentSettingFramebustBlockBubbleModel framebust_block_bubble_model(
      browser()->content_setting_bubble_model_delegate(), GetWebContents());

  EXPECT_FALSE(clicked_index_.has_value());
  EXPECT_FALSE(clicked_url_.has_value());

  content::TestNavigationObserver observer(GetWebContents());
  ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  framebust_block_bubble_model.OnListItemClicked(/* index = */ 1, click_event);
  observer.Wait();

  EXPECT_TRUE(clicked_index_.has_value());
  EXPECT_TRUE(clicked_url_.has_value());
  EXPECT_EQ(1u, clicked_index_.value());
  EXPECT_EQ(GURL(chrome::kChromeUISettingsURL), clicked_url_.value());
  EXPECT_FALSE(helper->HasBlockedUrls());
  EXPECT_EQ(blocked_urls[1], GetWebContents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(FramebustBlockBrowserTest, AllowRadioButtonSelected) {
  const GURL url = embedded_test_server()->GetURL("/iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Signal that a blocked redirection happened.
  auto* helper = GetFramebustTabHelper();
  helper->AddBlockedUrl(url, base::BindOnce(&FramebustBlockBrowserTest::OnClick,
                                            base::Unretained(this)));
  EXPECT_TRUE(helper->HasBlockedUrls());

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            settings_map->GetContentSetting(url, GURL(),
                                            ContentSettingsType::POPUPS));

  // Create a content bubble and simulate clicking on the first radio button
  // before closing it.
  ContentSettingFramebustBlockBubbleModel framebust_block_bubble_model(
      browser()->content_setting_bubble_model_delegate(), GetWebContents());
  std::unique_ptr<FakeOwner> owner = FakeOwner::Create(
      framebust_block_bubble_model, kDisallowRadioButtonIndex);

  owner->SetSelectedRadioOptionAndCommit(kAllowRadioButtonIndex);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            settings_map->GetContentSetting(url, GURL(),
                                            ContentSettingsType::POPUPS));
}

IN_PROC_BROWSER_TEST_F(FramebustBlockBrowserTest, DisallowRadioButtonSelected) {
  const GURL url = embedded_test_server()->GetURL("/iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Signal that a blocked redirection happened.
  auto* helper = GetFramebustTabHelper();
  helper->AddBlockedUrl(url, base::BindOnce(&FramebustBlockBrowserTest::OnClick,
                                            base::Unretained(this)));
  EXPECT_TRUE(helper->HasBlockedUrls());

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            settings_map->GetContentSetting(url, GURL(),
                                            ContentSettingsType::POPUPS));

  // Create a content bubble and simulate clicking on the second radio button
  // before closing it.
  ContentSettingFramebustBlockBubbleModel framebust_block_bubble_model(
      browser()->content_setting_bubble_model_delegate(), GetWebContents());

  std::unique_ptr<FakeOwner> owner =
      FakeOwner::Create(framebust_block_bubble_model, kAllowRadioButtonIndex);

  owner->SetSelectedRadioOptionAndCommit(kDisallowRadioButtonIndex);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            settings_map->GetContentSetting(url, GURL(),
                                            ContentSettingsType::POPUPS));
}

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_ManageButtonClicked DISABLED_ManageButtonClicked
#else
#define MAYBE_ManageButtonClicked ManageButtonClicked
#endif
IN_PROC_BROWSER_TEST_F(FramebustBlockBrowserTest, MAYBE_ManageButtonClicked) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
#endif

  const GURL url = embedded_test_server()->GetURL("/iframe.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Signal that a blocked redirection happened.
  auto* helper = GetFramebustTabHelper();
  helper->AddBlockedUrl(url, base::BindOnce(&FramebustBlockBrowserTest::OnClick,
                                            base::Unretained(this)));
  EXPECT_TRUE(helper->HasBlockedUrls());

  // Create a content bubble and simulate clicking on the second radio button
  // before closing it.
  ContentSettingFramebustBlockBubbleModel framebust_block_bubble_model(
      browser()->content_setting_bubble_model_delegate(), GetWebContents());

  content::TestNavigationObserver navigation_observer(nullptr);
  navigation_observer.StartWatchingNewWebContents();
  framebust_block_bubble_model.OnManageButtonClicked();
  navigation_observer.Wait();

  EXPECT_TRUE(base::StartsWith(navigation_observer.last_navigation_url().spec(),
                               chrome::kChromeUISettingsURL,
                               base::CompareCase::SENSITIVE));
}

IN_PROC_BROWSER_TEST_F(FramebustBlockBrowserTest, SimpleFramebust_Blocked) {
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/iframe.html"));

  GURL child_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  NavigateIframeToUrlWithoutGesture(GetWebContents(), "test", child_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(GetWebContents()->GetMainFrame(), 0);
  EXPECT_EQ(child_url, child->GetLastCommittedURL());

  GURL redirect_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  base::RunLoop block_waiter;
  blocked_url_added_closure_ = block_waiter.QuitClosure();
  child->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(base::StringPrintf("window.top.location = '%s';",
                                            redirect_url.spec().c_str())),
      base::NullCallback());
  block_waiter.Run();
  EXPECT_TRUE(
      base::Contains(GetFramebustTabHelper()->blocked_urls(), redirect_url));
}

IN_PROC_BROWSER_TEST_F(FramebustBlockBrowserTest,
                       FramebustAllowedByGlobalSetting) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                         CONTENT_SETTING_ALLOW);

  // Create a new browser to test in to ensure that the render process gets the
  // updated content settings.
  CreateAndSetBrowser();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/iframe.html"));
  NavigateIframeToUrlWithoutGesture(
      GetWebContents(), "test",
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  content::RenderFrameHost* child =
      content::ChildFrameAt(GetWebContents()->GetMainFrame(), 0);
  ASSERT_TRUE(child);

  GURL redirect_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  content::TestNavigationObserver observer(GetWebContents());
  child->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(base::StringPrintf("window.top.location = '%s';",
                                            redirect_url.spec().c_str())),
      base::NullCallback());
  observer.Wait();
  EXPECT_TRUE(GetFramebustTabHelper()->blocked_urls().empty());
}

IN_PROC_BROWSER_TEST_F(FramebustBlockBrowserTest,
                       FramebustAllowedBySiteSetting) {
  GURL top_level_url = embedded_test_server()->GetURL("/iframe.html");
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(top_level_url, GURL(),
                                              ContentSettingsType::POPUPS,
                                              CONTENT_SETTING_ALLOW);

  // Create a new browser to test in to ensure that the render process gets the
  // updated content settings.
  CreateAndSetBrowser();
  ui_test_utils::NavigateToURL(browser(), top_level_url);
  NavigateIframeToUrlWithoutGesture(
      GetWebContents(), "test",
      embedded_test_server()->GetURL("a.com", "/title1.html"));

  content::RenderFrameHost* child =
      content::ChildFrameAt(GetWebContents()->GetMainFrame(), 0);
  ASSERT_TRUE(child);

  GURL redirect_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  content::TestNavigationObserver observer(GetWebContents());
  child->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(base::StringPrintf("window.top.location = '%s';",
                                            redirect_url.spec().c_str())),
      base::NullCallback());
  observer.Wait();
  EXPECT_TRUE(GetFramebustTabHelper()->blocked_urls().empty());
}

// Regression test for https://crbug.com/894955, where the framebust UI would
// persist on subsequent navigations.
IN_PROC_BROWSER_TEST_F(FramebustBlockBrowserTest,
                       FramebustBlocked_SubsequentNavigation_NoUI) {
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/iframe.html"));

  GURL child_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  NavigateIframeToUrlWithoutGesture(GetWebContents(), "test", child_url);

  content::RenderFrameHost* child =
      content::ChildFrameAt(GetWebContents()->GetMainFrame(), 0);
  EXPECT_EQ(child_url, child->GetLastCommittedURL());

  GURL redirect_url = embedded_test_server()->GetURL("b.com", "/title1.html");

  base::RunLoop block_waiter;
  blocked_url_added_closure_ = block_waiter.QuitClosure();
  child->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(base::StringPrintf("window.top.location = '%s';",
                                            redirect_url.spec().c_str())),
      base::NullCallback());
  block_waiter.Run();
  EXPECT_TRUE(
      base::Contains(GetFramebustTabHelper()->blocked_urls(), redirect_url));

  // Now, navigate away and check that the UI went away.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title2.html"));

  // TODO(csharrison): Ideally we could query the actual UI here. For now, just
  // look at the internal state of the framebust tab helper.
  EXPECT_FALSE(GetFramebustTabHelper()->HasBlockedUrls());
}
