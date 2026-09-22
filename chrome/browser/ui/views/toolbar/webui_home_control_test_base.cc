// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_home_control_test_base.h"

#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "ui/base/clipboard/clipboard_url_info.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"

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

void WebUIHomeControlTestBase::TearDownOnMainThread() {
  // Both of these point into the browser window, which is about to go away.
  navigation_counter_.reset();
  webui_toolbar_view_ = nullptr;

  InProcessBrowserTest::TearDownOnMainThread();
}

GURL WebUIHomeControlTestBase::GetHomeURL() {
  GURL home_url(
      browser()->GetProfile()->GetPrefs()->GetString(prefs::kHomePage));
  if (home_url.is_empty()) {
    return chrome::ChromeUINewTabURLAsGURL();
  }
  return home_url;
}

void WebUIHomeControlTestBase::WaitForUndoBubble() {
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
               HomePageUndoBubbleCoordinator::kHomePageUndoBubbleMainViewId,
               views::ElementTrackerViews::GetContextForView(
                   webui_toolbar_view_)) != nullptr;
  }));
}

void WebUIHomeControlTestBase::PerformUndo() {
  views::View* bubble =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          HomePageUndoBubbleCoordinator::kHomePageUndoBubbleMainViewId,
          views::ElementTrackerViews::GetContextForView(webui_toolbar_view_));
  ASSERT_TRUE(bubble);
  auto* styled_label =
      static_cast<views::StyledLabel*>(bubble->children().front());
  styled_label->ClickFirstLinkForTesting();
}

content::WebContents* WebUIHomeControlTestBase::toolbar_web_contents() {
  return webui_toolbar_view_->GetWebViewForTesting()->GetWebContents();
}

content::WebContents* WebUIHomeControlTestBase::active_web_contents() {
  return browser()->GetTabStripModel()->GetActiveWebContents();
}

void WebUIHomeControlTestBase::SetUpHomeButtonDropTest() {
  // Start from a page that never navigates on its own, so that any navigation
  // observed later can only come from the drop.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  webui_toolbar_view_ = SetUpAndPinHomeButton(browser());
  ASSERT_TRUE(webui_toolbar_view_);
  webui_toolbar_view_->GetWidget()->LayoutRootViewIfNecessary();

  // The browser-side TrackedElement is created from a visibility report the
  // renderer sends asynchronously, which can land before the toolbar finishes
  // resizing after pinning the home button.
  ui::TrackedElement* home_button = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    webui_toolbar_view_->GetWidget()->LayoutRootViewIfNecessary();
    home_button = BrowserElements::From(browser())->GetElement(
        kToolbarHomeButtonElementId);
    if (!home_button || home_button->GetScreenBounds().IsEmpty()) {
      return false;
    }
    return webui_toolbar_view_->GetBoundsInScreen().Contains(
        home_button->GetScreenBounds());
  }));
  content::WaitForCopyableViewInWebContents(toolbar_web_contents());

  home_button_client_point_ =
      home_button->GetScreenBounds().CenterPoint() -
      webui_toolbar_view_->GetBoundsInScreen().OffsetFromOrigin();

  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  initial_home_page_ = prefs->GetString(prefs::kHomePage);
  initial_home_page_is_ntp_ = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);
  initial_tab_url_ = active_web_contents()->GetLastCommittedURL();
  navigation_counter_ =
      std::make_unique<NavigationCounter>(active_web_contents());
}

void WebUIHomeControlTestBase::PerformDropOnHomeButton(
    content::DropData drop_data) {
  const gfx::PointF client_pt(home_button_client_point_);
  const gfx::PointF screen_pt(
      home_button_client_point_ +
      webui_toolbar_view_->GetBoundsInScreen().OffsetFromOrigin());

  content::RenderWidgetHost* rwh =
      toolbar_web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  // Filter the drop data via `RenderWidgetHost::FilterDropData` before sending
  // `DragTargetDragEnter`, matching `WebContentsViewAura::DragEnteredCallback`.
  content::DropData filtered_drop_data = drop_data;
  rwh->FilterDropData(&filtered_drop_data);
  rwh->DragTargetDragEnter(filtered_drop_data, client_pt, screen_pt,
                           blink::kDragOperationEvery, /*key_modifiers=*/0,
                           base::DoNothing());

  // Pass the unfiltered drop data to `PreHandleDragUpdate` before sending
  // `DragTargetDragOver`, matching `WebContentsViewAura::DragUpdatedCallback`.
  toolbar_web_contents()->GetDelegate()->PreHandleDragUpdate(drop_data,
                                                             client_pt);
  base::test::TestFuture<ui::mojom::DragOperation, bool> drag_over_future;
  rwh->DragTargetDragOver(client_pt, screen_pt, blink::kDragOperationEvery,
                          /*key_modifiers=*/0, drag_over_future.GetCallback());
  ASSERT_TRUE(drag_over_future.Wait());
  EXPECT_NE(ui::mojom::DragOperation::kNone, drag_over_future.Get<0>());
  EXPECT_TRUE(drag_over_future.Get<1>());

  // Pass the filtered drop data through `HandleOnPerformingDrop` before
  // sending `DragTargetDrop`, matching
  // `WebContentsViewAura::PerformDropCallback`.
  filtered_drop_data.document_is_handling_drag = drag_over_future.Get<1>();
  base::test::TestFuture<std::optional<content::DropData>>
      performing_drop_future;
  HandleOnPerformingDrop(toolbar_web_contents(), std::move(filtered_drop_data),
                         performing_drop_future.GetCallback());
  std::optional<content::DropData> final_drop_data =
      performing_drop_future.Take();
  ASSERT_TRUE(final_drop_data.has_value());

  base::test::TestFuture<void> drop_future;
  rwh->DragTargetDrop(*final_drop_data, client_pt, screen_pt,
                      /*key_modifiers=*/0, drop_future.GetCallback());
  ASSERT_TRUE(drop_future.Wait());
}

void WebUIHomeControlTestBase::SimulateLinkDrop(const std::string& url,
                                                DragOrigin origin) {
  content::DropData drop_data;
  drop_data.did_originate_from_renderer = origin == DragOrigin::kWebPage;
  drop_data.url_infos.emplace_back(GURL(url), std::u16string());
  PerformDropOnHomeButton(drop_data);
}

void WebUIHomeControlTestBase::SimulateTextDrop(const std::string& text,
                                                DragOrigin origin) {
  content::DropData drop_data;
  drop_data.did_originate_from_renderer = origin == DragOrigin::kWebPage;
  drop_data.text = base::UTF8ToUTF16(text);
  PerformDropOnHomeButton(drop_data);
}

void WebUIHomeControlTestBase::SimulateLinkWithTextDrop(const std::string& url,
                                                        DragOrigin origin) {
  content::DropData drop_data;
  drop_data.did_originate_from_renderer = origin == DragOrigin::kWebPage;
  drop_data.url_infos.emplace_back(GURL(url), std::u16string());
  drop_data.text = base::UTF8ToUTF16(url);
  PerformDropOnHomeButton(drop_data);
}

void WebUIHomeControlTestBase::SimulateFileDrop(const base::FilePath& path,
                                                DragOrigin origin) {
  content::DropData drop_data;
  drop_data.did_originate_from_renderer = origin == DragOrigin::kWebPage;
  drop_data.filenames.emplace_back(path, path.BaseName());
  PerformDropOnHomeButton(drop_data);
}

void WebUIHomeControlTestBase::ExpectHomePageSetTo(const GURL& expected) {
  WaitForUndoBubble();

  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  EXPECT_EQ(expected.spec(), prefs->GetString(prefs::kHomePage));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

void WebUIHomeControlTestBase::ExpectNavigatedTo(const GURL& expected) {
  content::WebContents* active_contents = active_web_contents();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return active_contents->GetLastCommittedURL() == expected; }));

  ExpectHomePageUnchanged();
  ExpectNoUndoBubble();
}

void WebUIHomeControlTestBase::ExpectSearchedFor(const std::string& query) {
  content::WebContents* active_contents = active_web_contents();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    const GURL& url = active_contents->GetLastCommittedURL();
    return url.SchemeIs(url::kHttpsScheme) && url.host() == "www.google.com" &&
           url.path() == "/search";
  }));

  // Search terms are substituted into the query component, where
  // TemplateURLRef encodes spaces as '+'.
  std::string expected_query;
  base::ReplaceChars(query, " ", "+", &expected_query);
  EXPECT_NE(std::string_view::npos,
            active_contents->GetLastCommittedURL().query().find(
                "q=" + expected_query));

  ExpectHomePageUnchanged();
  ExpectNoUndoBubble();
}

void WebUIHomeControlTestBase::ExpectDropIgnored() {
  navigation_counter_->WaitForNoNavigations();
  EXPECT_EQ(initial_tab_url_, active_web_contents()->GetLastCommittedURL());

  ExpectHomePageUnchanged();
  ExpectNoUndoBubble();
}

void WebUIHomeControlTestBase::ExpectHomePageUnchanged() {
  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  EXPECT_EQ(initial_home_page_, prefs->GetString(prefs::kHomePage));
  EXPECT_EQ(initial_home_page_is_ntp_,
            prefs->GetBoolean(prefs::kHomePageIsNewTabPage));
}

void WebUIHomeControlTestBase::ExpectNoUndoBubble() {
  EXPECT_EQ(
      nullptr,
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          HomePageUndoBubbleCoordinator::kHomePageUndoBubbleMainViewId,
          views::ElementTrackerViews::GetContextForView(webui_toolbar_view_)));
}
