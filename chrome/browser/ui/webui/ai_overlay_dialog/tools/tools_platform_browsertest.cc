// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the AiOverlayTools behavior shared by desktop and Desktop Android.
// Desktop-only tools are covered by tools_browsertest.cc.

#include <memory>
#include <string>
#include <variant>

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ttc {
namespace {

using ToolResult = base::expected<std::monostate, std::string>;

constexpr char kTallPagePath[] = "/tall.html";

// Installs `window.scrollEnded`, a promise that resolves with `window.scrollY`
// once the next scroll of the document has finished.
constexpr char kWatchForScrollEnd[] = R"(
  window.scrollEnded = new Promise(resolve => {
    document.addEventListener('scrollend', () => resolve(window.scrollY),
                              {once: true});
  });
)";

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  if (request.GetURL().path() == kTallPagePath) {
    response->set_content(
        "<html><head><meta name='viewport' content='width=device-width'>"
        "</head><body style='height:10000px'>Tall page</body></html>");
  } else {
    response->set_content("<html><body>AiOverlayTools test page</body></html>");
  }
  return response;
}

class AiOverlayToolsPlatformBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(embedded_test_server()->Start());

    mojo::PendingRemote<ai_overlay_dialog::mojom::AiOverlayTools> remote;
    tools_ = AiOverlayTools::Create(remote.InitWithNewPipeAndPassReceiver(),
                                    GetBrowserWindowInterface(),
                                    /*page_context_monitor=*/nullptr);
  }

  void TearDownOnMainThread() override {
    tools_.reset();
    PlatformBrowserTest::TearDownOnMainThread();
  }

 protected:
  AiOverlayTools* tools() { return tools_.get(); }

  TabListInterface* tab_list() {
    return TabListInterface::From(GetBrowserWindowInterface());
  }

  content::WebContents* GetActiveWebContents() {
    tabs::TabInterface* tab = tab_list()->GetActiveTab();
    return tab ? tab->GetContents() : nullptr;
  }

  GURL GetTestUrl(const std::string& query) {
    return embedded_test_server()->GetURL("/page.html?" + query);
  }

  // Loads a page that is taller than the viewport and waits until it can
  // receive input.
  void NavigateToTallPage() {
    content::WebContents* contents = GetActiveWebContents();
    ASSERT_TRUE(content::NavigateToURL(
        contents, embedded_test_server()->GetURL(kTallPagePath)));
    content::WaitForHitTestData(contents->GetPrimaryMainFrame());
    ASSERT_EQ(0, content::EvalJs(contents, "scrollY").ExtractDouble());
  }

  // Runs the Scroll tool and returns `window.scrollY` once the resulting
  // scroll has finished.
  double ScrollAndWait(ai_overlay_dialog::mojom::ScrollGranularity granularity,
                       double magnitude) {
    content::WebContents* contents = GetActiveWebContents();
    // The installed promise only resolves after the scroll, so don't wait on
    // it here.
    EXPECT_TRUE(content::ExecJs(contents, kWatchForScrollEnd,
                                content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    base::test::TestFuture<ToolResult> future;
    tools()->Scroll(granularity, magnitude, future.GetCallback());
    EXPECT_TRUE(future.Get().has_value());
    return content::EvalJs(contents, "window.scrollEnded").ExtractDouble();
  }

 private:
  std::unique_ptr<AiOverlayTools> tools_;
};

IN_PROC_BROWSER_TEST_F(AiOverlayToolsPlatformBrowserTest, OpenUrlCurrentTab) {
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), GetTestUrl("initial")));
  const int initial_count = tab_list()->GetTabCount();
  const GURL target_url = GetTestUrl("target");

  content::TestNavigationObserver observer(GetActiveWebContents());
  base::test::TestFuture<ToolResult> future;
  tools()->OpenUrl(target_url.spec(), /*new_tab=*/false, future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
  observer.Wait();

  EXPECT_EQ(initial_count, tab_list()->GetTabCount());
  EXPECT_EQ(target_url, GetActiveWebContents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsPlatformBrowserTest, OpenUrlNewTab) {
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), GetTestUrl("initial")));
  const int initial_count = tab_list()->GetTabCount();
  const GURL target_url = GetTestUrl("target");

  content::TestNavigationObserver observer(target_url);
  observer.StartWatchingNewWebContents();
  base::test::TestFuture<ToolResult> future;
  tools()->OpenUrl(target_url.spec(), /*new_tab=*/true, future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
  observer.Wait();

  EXPECT_EQ(initial_count + 1, tab_list()->GetTabCount());
  EXPECT_EQ(target_url, GetActiveWebContents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsPlatformBrowserTest, OpenUrlInvalid) {
  base::test::TestFuture<ToolResult> future;
  tools()->OpenUrl("invalid_url", /*new_tab=*/true, future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("Invalid URL", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsPlatformBrowserTest,
                       OpenUrlBlocksNonHttpSchemes) {
  base::test::TestFuture<ToolResult> future;
  tools()->OpenUrl("file:///tmp/local_secret.txt", /*new_tab=*/true,
                   future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("Invalid URL", future.Get().error());
}

// GoBack acts on GetActiveWebContents(), so this also covers the shared
// active-tab lookup.
IN_PROC_BROWSER_TEST_F(AiOverlayToolsPlatformBrowserTest,
                       GoBackNavigatesActiveTab) {
  const GURL first_url = GetTestUrl("first");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), first_url));
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), GetTestUrl("second")));

  content::TestNavigationObserver observer(GetActiveWebContents());
  base::test::TestFuture<ToolResult> future;
  tools()->GoBack(future.GetCallback());
  EXPECT_TRUE(future.Get().has_value());
  observer.Wait();

  EXPECT_EQ(first_url, GetActiveWebContents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsPlatformBrowserTest, ScrollPage) {
  ASSERT_NO_FATAL_FAILURE(NavigateToTallPage());

  const double scroll_y =
      ScrollAndWait(ai_overlay_dialog::mojom::ScrollGranularity::kPage, 1.0);
  EXPECT_GT(scroll_y, 0);

  EXPECT_LT(
      ScrollAndWait(ai_overlay_dialog::mojom::ScrollGranularity::kPage, -1.0),
      scroll_y);
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsPlatformBrowserTest, ScrollDocument) {
  ASSERT_NO_FATAL_FAILURE(NavigateToTallPage());
  const double max_scroll_y =
      content::EvalJs(GetActiveWebContents(),
                      "document.scrollingElement.scrollHeight - innerHeight")
          .ExtractDouble();
  ASSERT_GT(max_scroll_y, 0);

  EXPECT_NEAR(ScrollAndWait(
                  ai_overlay_dialog::mojom::ScrollGranularity::kDocument, 1.0),
              max_scroll_y, 1);

  EXPECT_EQ(ScrollAndWait(
                ai_overlay_dialog::mojom::ScrollGranularity::kDocument, -1.0),
            0);
}

}  // namespace
}  // namespace ttc
