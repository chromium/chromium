// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/memory_pressure/fake_memory_pressure_monitor.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

using MakeContentsResult = WebUIContentsPreloadManager::MakeContentsResult;

class WebUIContentsPreloadManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  WebUIContentsPreloadManager* preload_manager() {
    return WebUIContentsPreloadManager::GetInstance();
  }

  void SetMemoryPressureLevel(
      base::MemoryPressureMonitor::MemoryPressureLevel level) {
    fake_memory_monitor_.SetAndNotifyMemoryPressure(level);
  }

 private:
  memory_pressure::test::FakeMemoryPressureMonitor fake_memory_monitor_;
  base::test::ScopedFeatureList enabled_feature_{
      features::kPreloadTopChromeWebUI};
};

TEST_F(WebUIContentsPreloadManagerTest, PreloadedContentsIsNullWithoutWarmup) {
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest, PreloadedContentsIsNotNullAfterWarmup) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->PreloadForBrowserContextForTesting(browser_context.get());
  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest, NoPreloadUnderHeavyMemoryPressure) {
  // Don't preload if the memory pressure is moderate or higher.
  SetMemoryPressureLevel(base::MemoryPressureMonitor::MemoryPressureLevel::
                             MEMORY_PRESSURE_LEVEL_MODERATE);
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->PreloadForBrowserContextForTesting(browser_context.get());
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest, MakeContentsReturnsNonNull) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  MakeContentsResult result = preload_manager()->MakeContents(
      GURL("about:blank"), browser_context.get());
  std::unique_ptr<content::WebContents> web_contents =
      std::move(result.web_contents);
  EXPECT_NE(web_contents, nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest,
       PreloadedContentsIsNotNullAfterMakeContents) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->MakeContents(GURL("about:blank"), browser_context.get());
  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest,
       PreloadedContentsChangesAfterSecondWarmupWithDifferentContext) {
  std::unique_ptr<content::BrowserContext> first_browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->PreloadForBrowserContextForTesting(
      first_browser_context.get());
  content::WebContents* first_preloaded_contents =
      preload_manager()->preloaded_web_contents();

  std::unique_ptr<content::BrowserContext> second_browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->PreloadForBrowserContextForTesting(
      second_browser_context.get());
  content::WebContents* second_preloaded_contents =
      preload_manager()->preloaded_web_contents();

  EXPECT_NE(first_preloaded_contents, second_preloaded_contents);
}

TEST_F(WebUIContentsPreloadManagerTest,
       WebContentsDiffersAfterWarmupThenMakeContentsWithDifferentContext) {
  std::unique_ptr<content::BrowserContext> first_browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->PreloadForBrowserContextForTesting(
      first_browser_context.get());
  content::WebContents* pre_warmup_web_contents =
      preload_manager()->preloaded_web_contents();

  std::unique_ptr<content::BrowserContext> second_browser_context =
      std::make_unique<TestingProfile>();
  MakeContentsResult result = preload_manager()->MakeContents(
      GURL("about:blank"), second_browser_context.get());
  std::unique_ptr<content::WebContents> made_web_contents =
      std::move(result.web_contents);

  EXPECT_NE(pre_warmup_web_contents, made_web_contents.get());
}

TEST_F(WebUIContentsPreloadManagerTest,
       WebContentsSameAfterWarmupThenMakeContentsWithSameContext) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->PreloadForBrowserContextForTesting(browser_context.get());
  content::WebContents* pre_warmup_web_contents =
      preload_manager()->preloaded_web_contents();

  MakeContentsResult result = preload_manager()->MakeContents(
      GURL("about:blank"), browser_context.get());

  std::unique_ptr<content::WebContents> made_web_contents =
      std::move(result.web_contents);

  EXPECT_EQ(pre_warmup_web_contents, made_web_contents.get());
}

TEST_F(WebUIContentsPreloadManagerTest,
       PreloadedContentsBecomesNullAfterProfileDestruction) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->PreloadForBrowserContextForTesting(browser_context.get());

  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);

  // Destroy the BrowserContext.
  browser_context.reset();

  // Now, check if the preloaded contents have been cleared and become nullptr.
  // This assumes that WebUIContentsPreloadManager listens to BrowserContext
  // destruction and acts accordingly.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
}

// Verify that calling MakeContents() navigates to the requested URL.
TEST_F(WebUIContentsPreloadManagerTest, MakeContentsNavigation) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  GURL preloaded_url = preload_manager()->GetPreloadedURLForTesting();
  preload_manager()->PreloadForBrowserContextForTesting(browser_context.get());

  // Case 1: MakeContents with the preloaded URL.
  {
    content::WebContents* preloaded_web_contents =
        preload_manager()->preloaded_web_contents();
    EXPECT_EQ(preloaded_web_contents->GetURL(), preloaded_url);

    MakeContentsResult result =
        preload_manager()->MakeContents(preloaded_url, browser_context.get());
    std::unique_ptr<content::WebContents> web_contents =
        std::move(result.web_contents);

    EXPECT_EQ(web_contents.get(), preloaded_web_contents);
  }

  // Case 2: MakeContents with a different URL.
  {
    GURL different_url("about:blank");
    EXPECT_NE(preloaded_url,
              different_url);  // Ensure the URL is indeed different.
    content::WebContents* preloaded_web_contents =
        preload_manager()->preloaded_web_contents();

    MakeContentsResult result =
        preload_manager()->MakeContents(different_url, browser_context.get());

    std::unique_ptr<content::WebContents> web_contents =
        std::move(result.web_contents);
    // WebContents is reused and navigated to the given URL.
    EXPECT_EQ(web_contents.get(), preloaded_web_contents);
    EXPECT_EQ(web_contents->GetURL(), different_url);
  }
}

// Test that MakeContentsResult::is_ready_to_show is initially false, and it
// becomes true after the preloaded WebUI calls
// MojoBubbleWebUIController::Embedder::ShowUI().
TEST_F(WebUIContentsPreloadManagerTest, IsReadyToShow) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->PreloadForBrowserContextForTesting(browser_context.get());
  GURL preloaded_url = preload_manager()->GetPreloadedURLForTesting();

  // `is_ready_to_show` should be initially false.
  MakeContentsResult result =
      preload_manager()->MakeContents(preloaded_url, browser_context.get());
  EXPECT_NE(result.web_contents, nullptr);
  EXPECT_FALSE(result.is_ready_to_show);

  content::WebContents* preloaded_web_contents =
      preload_manager()->preloaded_web_contents();
  ASSERT_NE(preloaded_web_contents, nullptr);

  // Simulate the WebUI calls into ShowUI().
  auto* webui_controller = static_cast<ui::MojoBubbleWebUIController*>(
      preloaded_web_contents->GetWebUI()->GetController());
  ASSERT_NE(webui_controller, nullptr);
  webui_controller->embedder()->ShowUI();

  // `is_ready_to_show` should be true after ShowUI() call.
  result =
      preload_manager()->MakeContents(preloaded_url, browser_context.get());
  EXPECT_TRUE(result.is_ready_to_show);
}
