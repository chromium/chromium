// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "ui/events/test/test_event.h"
#include "ui/views/test/widget_test.h"

class ContentSettingBubbleContentsInteractiveTest
    : public InProcessBrowserTest {
 public:
  ContentSettingBubbleContentsInteractiveTest()
      : prerender_helper_(base::BindRepeating(
            &ContentSettingBubbleContentsInteractiveTest::web_contents,
            base::Unretained(this))) {}
  ~ContentSettingBubbleContentsInteractiveTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ContentSettingImageView& GetContentSettingImageView(
      ContentSettingImageModel::ImageType image_type) {
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    return **base::ranges::find(
        location_bar_view->GetContentSettingViewsForTest(), image_type,
        &ContentSettingImageView::GetType);
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

class BubbleWidgetObserver : public views::WidgetObserver {
 public:
  explicit BubbleWidgetObserver(views::Widget* widget) : widget_(widget) {
    if (!widget_)
      return;
    widget_->AddObserver(this);
  }

  ~BubbleWidgetObserver() override { CleanupWidget(); }

  void WaitForClose() {
    if (!widget_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    CleanupWidget();
    if (run_loop_)
      run_loop_->Quit();
  }

  void CleanupWidget() {
    if (!widget_)
      return;
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }

 private:
  raw_ptr<views::Widget> widget_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(ContentSettingBubbleContentsInteractiveTest,
                       PrerenderDoesNotCloseBubble) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Accept any prompts so that content setting icons appear.
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);

  // Navigate to the test page.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "/content_setting_bubble/geolocation.html")));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  // Get the geolocation icon on the omnibox.
  ContentSettingImageView& geolocation_icon = GetContentSettingImageView(
      ContentSettingImageModel::ImageType::GEOLOCATION);
  // Geolocation icon should be off in the beginning.
  EXPECT_FALSE(geolocation_icon.GetVisible());

  // Access geolocation which will trigger a prompt which will be accepted
  permissions::PermissionRequestObserver request_observer(web_contents());
  ASSERT_TRUE(content::ExecJs(web_contents(), "geolocate();"));
  request_observer.Wait();

  // Geolocation icon should be on since geolocation API is used.
  EXPECT_TRUE(geolocation_icon.GetVisible());

  // Make sure its content setting bubble doesn't show yet.
  EXPECT_FALSE(geolocation_icon.IsBubbleShowing());

  // Click the geolocation icon.
  geolocation_icon.ShowBubble(ui::test::TestEvent());

  // Make sure its content setting bubble is shown.
  EXPECT_TRUE(geolocation_icon.IsBubbleShowing());

  // Start a prerender.
  auto prerender_url = embedded_test_server()->GetURL("/empty.html");
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  EXPECT_FALSE(host_observer.was_activated());

  // Make sure the bubble is still shown after prerender navigation.
  EXPECT_TRUE(geolocation_icon.IsBubbleShowing());

  BubbleWidgetObserver widget_close_observer(
      geolocation_icon.GetBubbleWidgetForTesting());

  // Activate the page from the prerendering.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());

  // Wait for the bubble widget to be closed.
  widget_close_observer.WaitForClose();

  // Make sure the bubble is not shown.
  EXPECT_FALSE(geolocation_icon.IsBubbleShowing());
}
