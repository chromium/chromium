// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

using ScrollResult = base::expected<bool, std::string>;
using PerformSearchResult = base::expected<bool, std::string>;
using FindAndHighlightResult = base::expected<bool, std::string>;
using PlayVideoResult = base::expected<bool, std::string>;
using PauseVideoResult = base::expected<bool, std::string>;
using SeekToTimestampResult = base::expected<bool, std::string>;
using OpenUrlResult = base::expected<bool, std::string>;
using SwitchTabResult =
    base::expected<ai_overlay_dialog::mojom::SwitchTabResultPtr, std::string>;
using CloseTabResult = base::expected<bool, std::string>;
using GoBackResult = base::expected<bool, std::string>;
using GoForwardResult = base::expected<bool, std::string>;
using ReloadResult = base::expected<bool, std::string>;

class AiOverlayToolsBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    mojo::PendingRemote<ai_overlay_dialog::mojom::AiOverlayTools> remote;
    tools_ = std::make_unique<AiOverlayTools>(
        remote.InitWithNewPipeAndPassReceiver(), browser());
  }

  void TearDownOnMainThread() override {
    tools_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  AiOverlayTools* tools() { return tools_.get(); }

  void AddTabWithTitle(const GURL& url, const std::string& title) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::ExecJs(contents, "document.title = '" + title + "'"));
  }

 private:
  std::unique_ptr<AiOverlayTools> tools_;
};

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenUrlNewTab) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html?initial");
  GURL target_url = embedded_test_server()->GetURL("/empty.html?target");

  AddTabWithTitle(initial_url, "Initial");
  int initial_count = browser()->tab_strip_model()->count();

  base::test::TestFuture<OpenUrlResult> future;
  tools()->OpenUrl(target_url.spec(), /*new_tab=*/true, future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value());
  EXPECT_EQ(initial_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(
      target_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenUrlCurrentTab) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html?initial");
  GURL target_url = embedded_test_server()->GetURL("/empty.html?target");

  AddTabWithTitle(initial_url, "Initial");
  int initial_count = browser()->tab_strip_model()->count();

  base::test::TestFuture<OpenUrlResult> future;
  tools()->OpenUrl(target_url.spec(), /*new_tab=*/false, future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value());
  EXPECT_EQ(initial_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(
      target_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, OpenUrlInvalid) {
  base::test::TestFuture<OpenUrlResult> future;
  tools()->OpenUrl("invalid_url", /*new_tab=*/true, future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("Invalid URL", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SwitchTabByTitle) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?2"),
                  "Second Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?3"), "Third Tab");

  int initial_active = browser()->tab_strip_model()->active_index();

  base::test::TestFuture<SwitchTabResult> future;
  tools()->SwitchTab("second", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(initial_active - 1, browser()->tab_strip_model()->active_index());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SwitchTabByUrl) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");
  GURL target_url = embedded_test_server()->GetURL("/empty.html?target_path");
  AddTabWithTitle(target_url, "Second Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?3"), "Third Tab");

  int initial_active = browser()->tab_strip_model()->active_index();

  base::test::TestFuture<SwitchTabResult> future;
  tools()->SwitchTab("target_path", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_EQ(target_url, future.Get().value()->url);
  EXPECT_EQ(initial_active - 1, browser()->tab_strip_model()->active_index());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, SwitchTabNotFound) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");

  base::test::TestFuture<SwitchTabResult> future;
  tools()->SwitchTab("nonexistent", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No matching tab found", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, CloseCurrentTab) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First Tab");
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?2"),
                  "Second Tab");

  int initial_count = browser()->tab_strip_model()->count();

  base::test::TestFuture<CloseTabResult> future;
  tools()->CloseCurrentTab(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value());
  EXPECT_EQ(initial_count - 1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoBack) {
  GURL first_url = embedded_test_server()->GetURL("/empty.html?1");
  GURL second_url = embedded_test_server()->GetURL("/empty.html?2");

  AddTabWithTitle(first_url, "First");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_url));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();

  EXPECT_TRUE(controller.CanGoBack());

  base::test::TestFuture<GoBackResult> future;
  tools()->GoBack(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoBackCannotGoBack) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First");
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();

  // Fresh tab has an initial navigation, wait wait, navigating once might add
  // an entry, but ui_test_utils::NavigateToURLWithDisposition replaces if
  // empty.
  EXPECT_FALSE(controller.CanGoBack());

  base::test::TestFuture<GoBackResult> future;
  tools()->GoBack(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("Cannot go back", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoForward) {
  GURL first_url = embedded_test_server()->GetURL("/empty.html?1");
  GURL second_url = embedded_test_server()->GetURL("/empty.html?2");

  AddTabWithTitle(first_url, "First");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_url));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();

  controller.GoBack();
  EXPECT_TRUE(content::WaitForLoadStop(contents));

  EXPECT_TRUE(controller.CanGoForward());

  base::test::TestFuture<GoForwardResult> future;
  tools()->GoForward(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, GoForwardCannotGoForward) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First");
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();

  EXPECT_FALSE(controller.CanGoForward());

  base::test::TestFuture<GoForwardResult> future;
  tools()->GoForward(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("Cannot go forward", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, ReloadPage) {
  AddTabWithTitle(embedded_test_server()->GetURL("/empty.html?1"), "First");

  base::test::TestFuture<ReloadResult> future;
  tools()->ReloadPage(future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, PerformSearch) {
  base::test::TestFuture<PerformSearchResult> future;
  tools()->PerformSearch("test query", /*new_tab=*/false, future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value());

  // Wait for navigation to complete. We just check the navigation works
  // as TemplateURLService provides the search URL.
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, Scroll) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<ScrollResult> future;
  tools()->Scroll(ai_overlay_dialog::mojom::ScrollGranularity::kPage, 1.0,
                  future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value());
}

// For media tools, we utilize the embedded test server's built-in
// video-with-metadata.html page to test active media sessions.

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, FindAndHighlightSuccess) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<html><body><p>This is a test paragraph "
                      "that we want to find.</p></body></html>")));

  base::test::TestFuture<FindAndHighlightResult> future;
  tools()->FindAndHighlight("test paragraph", future.GetCallback());

  EXPECT_TRUE(future.Get().has_value());
  EXPECT_TRUE(future.Get().value());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, FindAndHighlightNotFound) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<html><body><p>This is a test paragraph "
                      "that we want to find.</p></body></html>")));

  base::test::TestFuture<FindAndHighlightResult> future;
  tools()->FindAndHighlight("missing text", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No match found", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, PlayVideoNoMediaSession) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<PlayVideoResult> future;
  tools()->PlayVideo(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No active media session", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, VideoControls) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/media/session/video-with-metadata.html")));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Start playback.
  ASSERT_EQ(base::Value(), content::EvalJs(contents, "play()"));

  // Pause video
  {
    base::test::TestFuture<PauseVideoResult> future;
    tools()->PauseVideo(future.GetCallback());
    EXPECT_TRUE(future.Get().has_value());
    EXPECT_TRUE(future.Get().value());
  }

  // Play video
  {
    base::test::TestFuture<PlayVideoResult> future;
    tools()->PlayVideo(future.GetCallback());
    EXPECT_TRUE(future.Get().has_value());
    EXPECT_TRUE(future.Get().value());
  }

  // Seek
  {
    base::test::TestFuture<SeekToTimestampResult> future;
    tools()->SeekToTimestamp("0:05", future.GetCallback());
    EXPECT_TRUE(future.Get().has_value());
    EXPECT_TRUE(future.Get().value());
  }
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest, PauseVideoNoMediaSession) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<PauseVideoResult> future;
  tools()->PauseVideo(future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  EXPECT_EQ("No active media session", future.Get().error());
}

IN_PROC_BROWSER_TEST_F(AiOverlayToolsBrowserTest,
                       SeekToTimestampNoMediaSession) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));

  base::test::TestFuture<SeekToTimestampResult> future;
  tools()->SeekToTimestamp("0:30", future.GetCallback());

  EXPECT_FALSE(future.Get().has_value());
  // The media session isn't available, so it fails with this error before
  // parsing timecode.
  EXPECT_EQ("No active media session", future.Get().error());
}

}  // namespace
}  // namespace ttc
