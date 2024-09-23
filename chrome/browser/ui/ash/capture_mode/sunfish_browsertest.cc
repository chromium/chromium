// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#include "chrome/browser/ui/ash/capture_mode/search_results_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

std::unique_ptr<views::Widget> CreateWidget() {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  return std::make_unique<views::Widget>(std::move(params));
}

// Given an embedded script `body`, wraps it in a basic HTML structure, then
// returns a GURL accessible by the local test server.
GURL CreateDataUrlWithBody(const std::string& body) {
  return GURL(base::StringPrintf(R"(data:text/html,
      <html>
        <body>
          <style>
            * {
              margin: 0;
              padding: 0;
            }
          </style>
          %s
        </body>
      </html>
    )",
                                 body.c_str()));
}

namespace ash {

class SunfishBrowserTest : public InProcessBrowserTest {
 public:
  SunfishBrowserTest() = default;
  SunfishBrowserTest(const SunfishBrowserTest&) = delete;
  SunfishBrowserTest& operator=(const SunfishBrowserTest&) = delete;
  ~SunfishBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kSunfishFeature};
};

// Tests the basic functionalities of `SearchResultsView`.
IN_PROC_BROWSER_TEST_F(SunfishBrowserTest, SearchResultsView) {
  auto widget = CreateWidget();
  ChromeCaptureModeDelegate* delegate = ChromeCaptureModeDelegate::Get();
  auto* contents_view =
      widget->SetContentsView(delegate->CreateSearchResultsView());
  auto* search_results_view =
      views::AsViewClass<SearchResultsView>(contents_view);
  ASSERT_TRUE(search_results_view);
}

// Tests that links are opened in new tabs.
IN_PROC_BROWSER_TEST_F(SunfishBrowserTest, OpenLinksInNewTabs) {
  auto widget = CreateWidget();
  ChromeCaptureModeDelegate* delegate = ChromeCaptureModeDelegate::Get();
  auto* contents_view =
      widget->SetContentsView(delegate->CreateSearchResultsView());
  auto* search_results_view =
      views::AsViewClass<SearchResultsView>(contents_view);

  // Browser tests start out with 1 browser tab by default.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Simulate clicking on a new URL in the web view.
  auto* child_view =
      search_results_view->GetViewByID(kAshWebViewChildWebViewId);
  ASSERT_TRUE(child_view);
  auto* web_view = views::AsViewClass<views::WebView>(child_view);
  ASSERT_TRUE(web_view);
  auto* web_contents = web_view->web_contents();
  web_contents->GetController().LoadURL(
      CreateDataUrlWithBody(R"(
      <script>
        // Wait until window has finished loading.
        window.addEventListener("load", () => {

          // Perform simple click on an anchor within the same target.
          const anchor = document.createElement("a");
          anchor.href = "https://google.com/";
          anchor.click();

          // Wait for first click event to be flushed.
          setTimeout(() => {

            // Perform simple click on an anchor with "_blank" target.
            const anchor = document.createElement("a");
            anchor.href = "https://assistant.google.com/";
            anchor.target = "_blank";
            anchor.click();
          }, 0);
        });
      </script>
    )"),
      content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  content::TestNavigationObserver observer(web_contents);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Test it opens a new tab.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

}  // namespace ash
