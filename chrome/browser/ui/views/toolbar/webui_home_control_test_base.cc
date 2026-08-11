// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_home_control_test_base.h"

#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"

WebUIHomeControlTestBase::WebUIHomeControlTestBase() {
  feature_list_.InitWithFeatures(
      {features::kInitialWebUI, features::kWebUIHomeButton,
       features::kSkipIPCChannelPausingForNonGuests,
       features::kWebUIInProcessResourceLoadingV2},
      {});
}

WebUIHomeControlTestBase::~WebUIHomeControlTestBase() = default;

void WebUIHomeControlTestBase::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  ThemeServiceFactory::GetForProfile(browser()->GetProfile())
      ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);
}

GURL WebUIHomeControlTestBase::GetHomeURL() {
  GURL home_url(
      browser()->GetProfile()->GetPrefs()->GetString(prefs::kHomePage));
  if (home_url.is_empty()) {
    return chrome::ChromeUINewTabURLAsGURL();
  }
  return home_url;
}

void WebUIHomeControlTestBase::WaitForUndoBubble(
    WebUIToolbarWebView* webui_toolbar_view) {
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
               HomePageUndoBubbleCoordinator::kHomePageUndoBubbleMainViewId,
               views::ElementTrackerViews::GetContextForView(
                   webui_toolbar_view)) != nullptr;
  }));
}

void WebUIHomeControlTestBase::SimulateDropOnHomeButton(
    content::WebContents* web_contents,
    const std::string& url) {
  EXPECT_TRUE(content::ExecJs(web_contents,
                              base::StringPrintf(R"(
    const homeButton = document.querySelector('toolbar-app').shadowRoot
                           .querySelector('#home').shadowRoot
                           .querySelector('cr-icon-button');
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('text/uri-list', '%s');
    dataTransfer.setData('text/plain', '%s');
    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      dataTransfer: dataTransfer
    });
    homeButton.dispatchEvent(dropEvent);
  )",
                                                 url.c_str(), url.c_str())));
}

WebUIToolbarWebView* WebUIHomeControlTestBase::PerformDragAndDrop(
    const std::string& new_home_url) {
  WebUIToolbarWebView* webui_toolbar_view = SetUpAndPinHomeButton(browser());
  views::WebView* web_view = webui_toolbar_view->GetWebViewForTesting();

  SimulateDropOnHomeButton(web_view->GetWebContents(), new_home_url);

  // Wait for the bubble widget to be created.
  WaitForUndoBubble(webui_toolbar_view);

  // Verify the new home page was correctly set.
  auto* prefs = browser()->GetProfile()->GetPrefs();
  EXPECT_EQ(new_home_url, prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));

  return webui_toolbar_view;
}

void WebUIHomeControlTestBase::PerformUndo(
    WebUIToolbarWebView* webui_toolbar_view) {
  // Click undo.
  auto* bubble =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          HomePageUndoBubbleCoordinator::kHomePageUndoBubbleMainViewId,
          views::ElementTrackerViews::GetContextForView(webui_toolbar_view));
  ASSERT_TRUE(bubble);
  auto* styled_label =
      static_cast<views::StyledLabel*>(bubble->children().front());
  styled_label->ClickFirstLinkForTesting();
}
