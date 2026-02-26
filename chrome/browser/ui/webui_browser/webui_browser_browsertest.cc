// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui_browser/bookmark_bar.mojom.h"
#include "chrome/browser/ui/webui_browser/bookmark_bar_page_handler.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/surface_embed/buildflags/buildflags.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(ENABLE_SURFACE_EMBED)
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"
#include "components/surface_embed/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#endif  // BUILDFLAG(ENABLE_SURFACE_EMBED)

// Use an anonymous namespace here to avoid colliding with the other
// WebUIBrowserTest defined in chrome/test/base/ash/web_ui_browser_test.h
namespace {

class WebUIBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kWebium,
                              features::kAttachUnownedInnerWebContents},
        /*disabled_features=*/{});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FakeBookmarkBarPage : public bookmark_bar::mojom::Page {
 public:
  mojo::PendingRemote<bookmark_bar::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void BookmarkLoaded() override {}

  void FavIconChanged(
      bookmark_bar::mojom::BookmarkDataPtr bookmark_data) override {
    favicon_changed_ids_.push_back(bookmark_data->id);
  }

  void Show() override {}
  void Hide() override {}

  void FlushForTesting() { receiver_.FlushForTesting(); }

  std::vector<int64_t> favicon_changed_ids_;
  mojo::Receiver<bookmark_bar::mojom::Page> receiver_{this};
};

#if BUILDFLAG(ENABLE_SURFACE_EMBED)
// A WebUIBrowserTest with the SurfaceEmbed feature enabled and pixel output
// enabled for visual verification.
class WebUIBrowserSurfaceEmbedPixelTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    EnablePixelOutput();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kWebium,
                              surface_embed::features::kSurfaceEmbed},
        /*disabled_features=*/{});
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};
#endif  // BUILDFLAG(ENABLE_SURFACE_EMBED)

}  // namespace

// Ensures that WebUIBrowser does not crash on startup and can shutdown.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, StartupAndShutdown) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
}

// Verifies that WebUIBrowserWindow allows keyboard lock for tab WebContents.
// Tabs in WebUIBrowserWindow are inner WebContents, so this must return true
// for keyboard lock to work.
#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Fix and re-enable this test for CrOS.
#define MAYBE_AllowKeyboardLockForInnerContents \
  DISABLED_AllowKeyboardLockForInnerContents
#else
#define MAYBE_AllowKeyboardLockForInnerContents \
  AllowKeyboardLockForInnerContents
#endif
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest,
                       MAYBE_AllowKeyboardLockForInnerContents) {
  auto* capabilities = DesktopBrowserWindowCapabilities::From(browser());
  ASSERT_TRUE(capabilities);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(capabilities->AllowKeyboardLockForInnerContents(web_contents));
}

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Fix and re-enable this test for CrOS.
// For now this is disabled on CrOS since BrowserStatusMonitor/
// AppServiceInstanceRegistryHelper aren't happy with our shutdown deletion
// order of native windows vs. Browser and aren't tracking the switch over
// of views on child guest contents properly.
#define MAYBE_NavigatePage DISABLED_NavigatePage
#else
#define MAYBE_NavigatePage NavigatePage
#endif

// Navigation at chrome/ layer, which hits some focus management paths.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, MAYBE_NavigatePage) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Make sure that the web contents actually got converted to a guest before
  // we navigate it again, so that WebContentsViewChildFrame gets involved.
  EXPECT_TRUE(base::test::RunUntil([web_contents]() {
    return web_contents->GetOuterWebContents() != nullptr;
  }));

  GURL url = embedded_https_test_server().GetURL("a.com", "/defaultresponse");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("Default response given for path: /defaultresponse",
            EvalJs(web_contents, "document.body.textContent"));
}

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Fix and re-enable this test for CrOS.
// For now this is disabled on CrOS since BrowserStatusMonitor/
// AppServiceInstanceRegistryHelper aren't happy with our shutdown deletion
// order of native windows vs. Browser and aren't tracking the switch over
// of views on child guest contents properly.
#define MAYBE_EnumerateDevToolsTargets DISABLED_EnumerateDevToolsTargets
#else
#define MAYBE_EnumerateDevToolsTargets EnumerateDevToolsTargets
#endif
// Verify DevTools targets enumeration for browser UI and tabs.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, MAYBE_EnumerateDevToolsTargets) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Make sure that the web contents actually got converted to a guest and in
  // DOM before enumerate DevTools targets.
  EXPECT_TRUE(base::test::RunUntil([web_contents]() {
    return web_contents->GetOuterWebContents() != nullptr;
  }));

  // Verify DevTools target types.
  auto targets = content::DevToolsAgentHost::GetOrCreateAll();
  int tab_count = 0;
  int page_count = 0;
  int browser_ui_count = 0;
  auto hosts = content::DevToolsAgentHost::GetOrCreateAll();
  for (auto& host : hosts) {
    LOG(INFO) << "Found DevTools target, type: " << host->GetType()
              << ", parent id:" << host->GetParentId()
              << ", url: " << host->GetURL().spec();
    // Only expect top level targets.
    EXPECT_TRUE(host->GetParentId().empty());
    if (host->GetType() == content::DevToolsAgentHost::kTypeTab) {
      ++tab_count;
    } else if (host->GetType() == content::DevToolsAgentHost::kTypePage) {
      ++page_count;
    } else if (host->GetType() == content::DevToolsAgentHost::kTypeBrowserUI) {
      ++browser_ui_count;
    }
  }
  // Expect browser_ui target for browser UI main frame, Tab target for tab
  // WebContents, and Page target for tab main frame.
  EXPECT_EQ(hosts.size(), 3U);
  EXPECT_EQ(browser_ui_count, 1);
  EXPECT_EQ(tab_count, 1);
  EXPECT_EQ(page_count, 1);
}

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Fix and re-enable this test for CrOS.
#define MAYBE_FullscreenEnterAndExit DISABLED_FullscreenEnterAndExit
#else
#define MAYBE_FullscreenEnterAndExit FullscreenEnterAndExit
#endif
// Test entering and exiting fullscreen mode.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, MAYBE_FullscreenEnterAndExit) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Should not be in fullscreen initially.
  EXPECT_FALSE(window->IsFullscreen());

  // Enter fullscreen mode.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(window->IsFullscreen());

  // Exit fullscreen mode.
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  EXPECT_FALSE(window->IsFullscreen());
}

#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Fix and re-enable this test for CrOS.
#define MAYBE_TabFullscreenEnterAndExit DISABLED_TabFullscreenEnterAndExit
#else
#define MAYBE_TabFullscreenEnterAndExit TabFullscreenEnterAndExit
#endif
// Test entering and exiting tab fullscreen mode, including tab switching.
IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, MAYBE_TabFullscreenEnterAndExit) {
  auto* window = browser()->window();
  ASSERT_TRUE(window);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Add a second tab.
  GURL url = embedded_https_test_server().GetURL("a.com", "/defaultresponse");
  EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  content::WebContents* second_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(second_tab);
  ASSERT_NE(web_contents, second_tab);

  auto* fullscreen_controller = browser()
                                    ->GetFeatures()
                                    .exclusive_access_manager()
                                    ->fullscreen_controller();

  // Enter tab fullscreen mode on second tab.
  fullscreen_controller->EnterFullscreenModeForTab(
      second_tab->GetPrimaryMainFrame());

  // Wait for fullscreen state.
  EXPECT_TRUE(
      base::test::RunUntil([window]() { return window->IsFullscreen(); }));
  EXPECT_TRUE(window->IsFullscreen());

  // Exit fullscreen explicitly before switching tabs.
  fullscreen_controller->ExitFullscreenModeForTab(second_tab);
  EXPECT_TRUE(
      base::test::RunUntil([window]() { return !window->IsFullscreen(); }));
  EXPECT_FALSE(window->IsFullscreen());

  // Switch to first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_FALSE(window->IsFullscreen());

  // Switch back to the second tab.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_FALSE(window->IsFullscreen());

  // Enter fullscreen again on the second tab.
  fullscreen_controller->EnterFullscreenModeForTab(
      second_tab->GetPrimaryMainFrame());
  EXPECT_TRUE(
      base::test::RunUntil([window]() { return window->IsFullscreen(); }));
  EXPECT_TRUE(window->IsFullscreen());

  // Exit fullscreen.
  fullscreen_controller->ExitFullscreenModeForTab(second_tab);
  EXPECT_TRUE(
      base::test::RunUntil([window]() { return !window->IsFullscreen(); }));
  EXPECT_FALSE(window->IsFullscreen());
}

IN_PROC_BROWSER_TEST_F(WebUIBrowserTest, BookmarkNodeFaviconChangedRegression) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(base::test::RunUntil([&]() { return model->loaded(); }));

  FakeBookmarkBarPage page;
  mojo::PendingRemote<bookmark_bar::mojom::PageHandler> page_handler;
  WebUIBrowserBookmarkBarPageHandler handler(
      page_handler.InitWithNewPipeAndPassReceiver(), page.BindAndGetRemote(),
      /*web_ui=*/nullptr, browser());

  const bookmarks::BookmarkNode* other_child =
      model->AddURL(model->other_node(), 0, u"Other", GURL("http://other.com"));
  const bookmarks::BookmarkNode* bar_child = model->AddURL(
      model->bookmark_bar_node(), 0, u"Bar", GURL("http://bar.com"));

  // Trigger favicon loads. This will asynchronously notify observers.
  model->GetFavicon(other_child);
  model->GetFavicon(bar_child);

  // Wait for both favicons to finish loading.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return other_child->is_favicon_loaded() && bar_child->is_favicon_loaded();
  }));

  // Flush any pending mojo messages to ensure we've received all IPCs.
  page.FlushForTesting();

  // We expect FavIconChanged to be called for `bar_child`, but NOT for
  // `other_child` since it is not a direct child of the bookmark bar.
  EXPECT_EQ(page.favicon_changed_ids_.size(), 1u);
  if (!page.favicon_changed_ids_.empty()) {
    EXPECT_EQ(page.favicon_changed_ids_[0], bar_child->id());
  }
}

#if BUILDFLAG(ENABLE_SURFACE_EMBED)
#if BUILDFLAG(IS_CHROMEOS)
// TODO(crbug.com/451876195): Enable this test for CrOS.
#define MAYBE_SurfaceEmbedRendersRedRect DISABLED_SurfaceEmbedRendersRedRect
#else
#define MAYBE_SurfaceEmbedRendersRedRect SurfaceEmbedRendersRedRect
#endif  // BUILDFLAG(IS_CHROMEOS)

// Verifies that when kSurfaceEmbed is enabled, the WebUI browser (Webium)
// renders a red rectangle for the tab content. This test will need updated as
// surface embed support is expanded.
IN_PROC_BROWSER_TEST_F(WebUIBrowserSurfaceEmbedPixelTest,
                       MAYBE_SurfaceEmbedRendersRedRect) {
  // Get the UI WebContents (the embedder/outer frame that contains the <embed>
  // element with the SurfaceEmbedWebPlugin). We need to capture from this
  // WebContents since it has the fully composed view including the plugin's
  // red rectangle. The tab's WebContents won't be included in the rendered
  // output until SurfaceEmbedConnector connects up the visual output.
  content::WebContents* ui_web_contents =
      WebUIBrowserWindow::FromBrowser(browser())
          ->GetWebUIBrowserUI()
          ->web_ui()
          ->GetWebContents();

  EXPECT_TRUE(content::WaitForLoadStop(ui_web_contents));

  // Attempt to capture pixels from the WebContents until we get the expected
  // output color. The test will timeout if that doesn't happen.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    bool found_expected_color = false;
    base::RunLoop run_loop;
    gfx::Rect capture_rect(200, 200, 5, 5);
    ui_web_contents->GetPrimaryMainFrame()->GetView()->CopyFromSurface(
        capture_rect, capture_rect.size(), base::TimeDelta(),
        base::BindLambdaForTesting(
            [&](const content::CopyFromSurfaceResult& result) {
              if (result.has_value() && !result.value().bitmap.empty() &&
                  result.value().bitmap.getColor(0, 0) == SK_ColorRED) {
                found_expected_color = true;
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return found_expected_color;
  }));
}
#endif  // BUILDFLAG(ENABLE_SURFACE_EMBED)
