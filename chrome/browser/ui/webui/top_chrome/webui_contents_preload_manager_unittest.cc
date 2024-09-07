// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/top_chrome/preload_candidate_selector.h"
#include "chrome/browser/ui/webui/top_chrome/preload_context.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager_test_api.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/memory_pressure/fake_memory_pressure_monitor.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;
using RequestResult = WebUIContentsPreloadManager::RequestResult;

namespace {

template <typename T>
T ExpectHasValue(std::optional<T> optional) {
  EXPECT_TRUE(optional.has_value());
  return *optional;
}

class MockPreloadCandidateSelector : public webui::PreloadCandidateSelector {
 public:
  MOCK_METHOD(void, Init, (const std::vector<GURL>&), (override));
  MOCK_METHOD(std::optional<GURL>,
              GetURLToPreload,
              (const webui::PreloadContext&),
              (const, override));
};

}  // namespace

class WebUIContentsPreloadManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  WebUIContentsPreloadManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Always preload Tab Search.
    auto preload_candidate_selector =
        std::make_unique<testing::NiceMock<MockPreloadCandidateSelector>>();
    preload_candidate_selector_ = preload_candidate_selector.get();
    test_api().SetPreloadCandidateSelector(
        std::move(preload_candidate_selector));
    ON_CALL(*preload_candidate_selector_, GetURLToPreload(_))
        .WillByDefault(Return(GURL(chrome::kChromeUITabSearchURL)));
  }
  void TearDown() override {
    preload_candidate_selector_ = nullptr;
    // The mock object does not expect itself to leak outside of the test.
    // Clearing it from the preload manager to destroy it.
    test_api().SetPreloadCandidateSelector(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  WebUIContentsPreloadManager* preload_manager() {
    return WebUIContentsPreloadManager::GetInstance();
  }

  void SetMemoryPressureLevel(
      base::MemoryPressureMonitor::MemoryPressureLevel level) {
    fake_memory_monitor_.SetAndNotifyMemoryPressure(level);
  }

  MockPreloadCandidateSelector& preload_candidate_selector() {
    return *preload_candidate_selector_;
  }

  WebUIContentsPreloadManagerTestAPI& test_api() { return test_api_; }

 private:
  memory_pressure::test::FakeMemoryPressureMonitor fake_memory_monitor_;
  base::test::ScopedFeatureList enabled_feature_{
      features::kPreloadTopChromeWebUI};
  WebUIContentsPreloadManagerTestAPI test_api_;
  raw_ptr<MockPreloadCandidateSelector> preload_candidate_selector_;
};

TEST_F(WebUIContentsPreloadManagerTest, PreloadedContentsIsNullWithoutWarmup) {
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest, PreloadedContentsIsNotNullAfterWarmup) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  test_api().MaybePreloadForBrowserContext(browser_context.get());
  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest, NoPreloadUnderHeavyMemoryPressure) {
  // Don't preload if the memory pressure is moderate or higher.
  SetMemoryPressureLevel(base::MemoryPressureMonitor::MemoryPressureLevel::
                             MEMORY_PRESSURE_LEVEL_MODERATE);
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  test_api().MaybePreloadForBrowserContext(browser_context.get());
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest, RequestReturnsNonNull) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  RequestResult result =
      preload_manager()->Request(GURL("about:blank"), browser_context.get());
  std::unique_ptr<content::WebContents> web_contents =
      std::move(result.web_contents);
  EXPECT_NE(web_contents, nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest,
       PreloadedContentsIsNotNullAfterRequest) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  preload_manager()->Request(GURL("about:blank"), browser_context.get());
  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);
}

TEST_F(WebUIContentsPreloadManagerTest,
       PreloadedContentsChangesAfterSecondWarmupWithDifferentContext) {
  std::unique_ptr<content::BrowserContext> first_browser_context =
      std::make_unique<TestingProfile>();
  test_api().MaybePreloadForBrowserContext(first_browser_context.get());
  content::WebContents* first_preloaded_contents =
      preload_manager()->preloaded_web_contents();

  std::unique_ptr<content::BrowserContext> second_browser_context =
      std::make_unique<TestingProfile>();
  test_api().MaybePreloadForBrowserContext(second_browser_context.get());
  content::WebContents* second_preloaded_contents =
      preload_manager()->preloaded_web_contents();

  EXPECT_NE(first_preloaded_contents, second_preloaded_contents);
}

TEST_F(WebUIContentsPreloadManagerTest,
       WebContentsDiffersAfterWarmupThenRequestWithDifferentContext) {
  std::unique_ptr<content::BrowserContext> first_browser_context =
      std::make_unique<TestingProfile>();
  test_api().MaybePreloadForBrowserContext(first_browser_context.get());
  content::WebContents* pre_warmup_web_contents =
      preload_manager()->preloaded_web_contents();

  std::unique_ptr<content::BrowserContext> second_browser_context =
      std::make_unique<TestingProfile>();
  RequestResult result = preload_manager()->Request(
      GURL("about:blank"), second_browser_context.get());
  std::unique_ptr<content::WebContents> made_web_contents =
      std::move(result.web_contents);

  EXPECT_NE(pre_warmup_web_contents, made_web_contents.get());
}

TEST_F(WebUIContentsPreloadManagerTest,
       WebContentsSameAfterWarmupThenRequestWithSameContext) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  test_api().MaybePreloadForBrowserContext(browser_context.get());
  content::WebContents* pre_warmup_web_contents =
      preload_manager()->preloaded_web_contents();

  RequestResult result =
      preload_manager()->Request(GURL("about:blank"), browser_context.get());

  std::unique_ptr<content::WebContents> made_web_contents =
      std::move(result.web_contents);

  EXPECT_EQ(pre_warmup_web_contents, made_web_contents.get());
}

TEST_F(WebUIContentsPreloadManagerTest,
       PreloadedContentsBecomesNullAfterProfileDestruction) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  test_api().MaybePreloadForBrowserContext(browser_context.get());

  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);

  // Destroy the BrowserContext.
  browser_context.reset();

  // Now, check if the preloaded contents have been cleared and become nullptr.
  // This assumes that WebUIContentsPreloadManager listens to BrowserContext
  // destruction and acts accordingly.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
}

// Verify that calling Request() navigates to the requested URL.
TEST_F(WebUIContentsPreloadManagerTest, RequestNavigation) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  GURL url_to_preload =
      *(test_api().GetNextWebUIURLToPreload(browser_context.get()));
  test_api().MaybePreloadForBrowserContext(browser_context.get());

  // Case 1: Request with the preloaded URL.
  {
    content::WebContents* preloaded_web_contents =
        preload_manager()->preloaded_web_contents();
    EXPECT_EQ(preloaded_web_contents->GetURL(), url_to_preload);

    RequestResult result =
        preload_manager()->Request(url_to_preload, browser_context.get());
    std::unique_ptr<content::WebContents> web_contents =
        std::move(result.web_contents);

    EXPECT_EQ(web_contents.get(), preloaded_web_contents);
  }

  // Case 2: Request with a different URL.
  {
    GURL different_url("about:blank");
    EXPECT_NE(url_to_preload,
              different_url);  // Ensure the URL is indeed different.
    content::WebContents* preloaded_web_contents =
        preload_manager()->preloaded_web_contents();

    RequestResult result =
        preload_manager()->Request(different_url, browser_context.get());

    std::unique_ptr<content::WebContents> web_contents =
        std::move(result.web_contents);
    // WebContents is reused and navigated to the given URL.
    EXPECT_EQ(web_contents.get(), preloaded_web_contents);
    EXPECT_EQ(web_contents->GetURL(), different_url);
  }
}

// Test that RequestResult::is_ready_to_show is initially false, and it
// becomes true after the preloaded WebUI calls
// TopChromeWebUIController::Embedder::ShowUI().
TEST_F(WebUIContentsPreloadManagerTest, IsReadyToShow) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  test_api().MaybePreloadForBrowserContext(browser_context.get());
  GURL preloaded_url = ExpectHasValue(test_api().GetPreloadedURL());

  // `is_ready_to_show` should be initially false.
  RequestResult result =
      preload_manager()->Request(preloaded_url, browser_context.get());
  EXPECT_NE(result.web_contents, nullptr);
  EXPECT_FALSE(result.is_ready_to_show);

  content::WebContents* preloaded_web_contents =
      preload_manager()->preloaded_web_contents();
  ASSERT_NE(preloaded_web_contents, nullptr);

  // Simulate the WebUI calls into ShowUI().
  auto* webui_controller = static_cast<TopChromeWebUIController*>(
      preloaded_web_contents->GetWebUI()->GetController());
  ASSERT_NE(webui_controller, nullptr);
  webui_controller->embedder()->ShowUI();

  // `is_ready_to_show` should be true after ShowUI() call.
  result = preload_manager()->Request(preloaded_url, browser_context.get());
  EXPECT_TRUE(result.is_ready_to_show);
}

// Regression test for crbug.com/329954901.
TEST_F(WebUIContentsPreloadManagerTest, RequestThenWarmupShouldNotCrash) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  std::unique_ptr<content::BrowserContext> browser_context2 =
      std::make_unique<TestingProfile>();
  const GURL url_to_preload =
      *(test_api().GetNextWebUIURLToPreload(browser_context.get()));

  RequestResult result =
      preload_manager()->Request(url_to_preload, browser_context.get());
  // Preload for a different browser context.
  test_api().MaybePreloadForBrowserContext(browser_context2.get());
}

// Tests that the preload manager preloads the WebUI decided by the candidate
// selector.
TEST_F(WebUIContentsPreloadManagerTest, CandidateSelector) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  const GURL url1("chrome://example1"), url2("chrome://example2");

  ON_CALL(preload_candidate_selector(), GetURLToPreload(_))
      .WillByDefault(Return(url1));
  test_api().MaybePreloadForBrowserContext(browser_context.get());
  EXPECT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(), url1);

  ON_CALL(preload_candidate_selector(), GetURLToPreload(_))
      .WillByDefault(Return(url2));
  RequestResult result =
      preload_manager()->Request(url1, browser_context.get());
  EXPECT_EQ(result.web_contents->GetVisibleURL(), url1);
  EXPECT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(), url2);
}

// Tests that WebUI destroy may trigger new preloading.
TEST_F(WebUIContentsPreloadManagerTest, PreloadOnWebUIDestroy) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  const GURL url1("chrome://example1"), url2("chrome://example2");

  // URL1 is preferred over URL2.
  ON_CALL(preload_candidate_selector(), GetURLToPreload(_))
      .WillByDefault(Return(url1));
  test_api().MaybePreloadForBrowserContext(browser_context.get());
  // Initially, URL1 is preloaded.
  EXPECT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(), url1);

  // Now, show URL1, then URL2 is preloaded.
  ON_CALL(preload_candidate_selector(), GetURLToPreload(_))
      .WillByDefault(Return(url2));
  RequestResult result =
      preload_manager()->Request(url1, browser_context.get());
  EXPECT_EQ(result.web_contents->GetVisibleURL(), url1);
  EXPECT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(), url2);

  // Destroy URL1. Since URL1 is preferred over URL2, URL1 should be preloaded.
  ON_CALL(preload_candidate_selector(), GetURLToPreload(_))
      .WillByDefault(Return(url1));
  result.web_contents.reset();
  EXPECT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(), url1);
}

// Tests that `Request(url)` retains the url path if it exists.
TEST_F(WebUIContentsPreloadManagerTest, RequestURLHasPath) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  const GURL url1("chrome://example1"), url2("chrome://example2");
  ON_CALL(preload_candidate_selector(), GetURLToPreload(_))
      .WillByDefault(Return(url1));
  test_api().MaybePreloadForBrowserContext(browser_context.get());

  // Case 1: request a WebUI that is preloaded.
  {
    EXPECT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(),
              url1);
    const GURL url1_with_path = url1.Resolve("path");
    RequestResult result =
        preload_manager()->Request(url1_with_path, browser_context.get());
    EXPECT_EQ(result.web_contents->GetVisibleURL(), url1_with_path);
  }

  // Case 2: request a WebUI that is not preloaded.
  {
    EXPECT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(),
              url1);
    const GURL url2_with_path = url2.Resolve("path");
    RequestResult result =
        preload_manager()->Request(url2_with_path, browser_context.get());
    EXPECT_EQ(result.web_contents->GetVisibleURL(), url2_with_path);
  }
}

// Tests that the preloading does not happen until the observed web contents
// emits the non-empty paint.
TEST_F(WebUIContentsPreloadManagerTest, DelayPreloadUntilNonEmptyPaint) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  std::unique_ptr<content::WebContents> test_web_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context.get(),
                                                        nullptr);
  test_api().MaybePreloadForBrowserContextLater(
      browser_context.get(), test_web_contents.get(), base::TimeDelta::Max());
  // Not yet preload.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
  // Notify the first non-empty paint.
  content::WebContentsTester::For(test_web_contents.get())
      ->TestDidFirstVisuallyNonEmptyPaint();
  // Now it's preloaded.
  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);
}

// Tests that the preloading does not happen until the deadline has passed.
TEST_F(WebUIContentsPreloadManagerTest, DelayPreloadUntilDeadline) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  const base::TimeDelta deadline = base::Seconds(3);
  test_api().MaybePreloadForBrowserContextLater(browser_context.get(), nullptr,
                                                deadline);
  // Not yet preload.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
  // Fast forward time to pass deadline.
  task_environment()->FastForwardBy(deadline);
  // Now it's preloaded.
  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);
}

// Tests that a pending preload is cancelled when a new preload comes in.
TEST_F(WebUIContentsPreloadManagerTest, PendingDelayCancelDueToNewPreload) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  std::unique_ptr<content::WebContents> test_web_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context.get(),
                                                        nullptr);
  std::unique_ptr<content::BrowserContext> browser_context2 =
      std::make_unique<TestingProfile>();
  std::unique_ptr<content::WebContents> test_web_contents2 =
      content::WebContentsTester::CreateTestWebContents(browser_context2.get(),
                                                        nullptr);
  test_api().MaybePreloadForBrowserContextLater(
      browser_context.get(), test_web_contents.get(), base::TimeDelta::Max());
  // Not yet preload.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
  // Preload under another profile.
  test_api().MaybePreloadForBrowserContextLater(
      browser_context2.get(), test_web_contents2.get(), base::TimeDelta::Max());
  // Notify the 1st non-empty paint of the first web contents.
  content::WebContentsTester::For(test_web_contents.get())
      ->TestDidFirstVisuallyNonEmptyPaint();
  // Still not preloaded.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
  // Notify the 2nd non-empty paint of the first web contents.
  content::WebContentsTester::For(test_web_contents2.get())
      ->TestDidFirstVisuallyNonEmptyPaint();
  // Now it's preloaded.
  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);
  EXPECT_EQ(preload_manager()->preloaded_web_contents()->GetBrowserContext(),
            browser_context2.get());
}

// Tests that a pending preload is cancelled when its profile is destroying.
TEST_F(WebUIContentsPreloadManagerTest, PendingDelayCancelDueToProfileDestroy) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  std::unique_ptr<content::WebContents> test_web_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context.get(),
                                                        nullptr);
  const base::TimeDelta deadline = base::Seconds(3);
  test_api().MaybePreloadForBrowserContextLater(
      browser_context.get(), test_web_contents.get(), deadline);
  // Not yet preload.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
  // Destroy the profile.
  test_web_contents.reset();
  browser_context.reset();
  // Fast forward time to pass deadline.
  task_environment()->FastForwardBy(deadline);
  // Still not preloaded.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
}

// Tests that delayed preload is eventually fired only once due to non-empty
// paint or deadline passing, but not both.
TEST_F(WebUIContentsPreloadManagerTest, DelayPreloadFireOnce) {
  std::unique_ptr<content::BrowserContext> browser_context =
      std::make_unique<TestingProfile>();
  std::unique_ptr<content::WebContents> test_web_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context.get(),
                                                        nullptr);
  const base::TimeDelta deadline = base::Seconds(3);

  // == 1st preload ==
  test_api().MaybePreloadForBrowserContextLater(
      browser_context.get(), test_web_contents.get(), deadline);
  // Not yet preload.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
  // Notify the first non-empty paint.
  content::WebContentsTester::For(test_web_contents.get())
      ->TestDidFirstVisuallyNonEmptyPaint();
  // Now it's preloaded.
  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);

  // Clear preloaded contents.
  test_api().SetPreloadedContents(nullptr);
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);

  // Fast forward to pass deadline should not trigger preload.
  task_environment()->FastForwardBy(deadline);
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);

  // == 2nd preload ==
  test_api().MaybePreloadForBrowserContextLater(
      browser_context.get(), test_web_contents.get(), deadline);
  // Not yet preload.
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
  // Fast forward to pass deadline should trigger preload.
  task_environment()->FastForwardBy(deadline);
  EXPECT_NE(preload_manager()->preloaded_web_contents(), nullptr);

  // Clear preloaded contents.
  test_api().SetPreloadedContents(nullptr);
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);

  // The first non-empty paint should not trigger preload.
  content::WebContentsTester::For(test_web_contents.get())
      ->TestDidFirstVisuallyNonEmptyPaint();
  EXPECT_EQ(preload_manager()->preloaded_web_contents(), nullptr);
}
