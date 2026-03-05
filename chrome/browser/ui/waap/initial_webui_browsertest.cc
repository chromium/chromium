// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/common/webui_url_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace waap {

namespace {

// Initializes a web ui controller after navigation. Note that this is probably
// an indication that these tests require too much insight into the state of
// inner implementation details, or there is some deficiency in the testing
// framework. Perhaps these tests should be promoted to ui tests.
class WebUIControllerInitalizer : protected content::WebContentsObserver {
 public:
  ~WebUIControllerInitalizer() override = default;

  virtual void Init(content::WebUIController* web_ui_controller) = 0;
  void Watch(content::WebContents* web_contents) {
    content::WebContentsObserver::Observe(web_contents);
  }

 protected:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    auto* controller = handle->GetWebContents()->GetWebUI()->GetController();
    Init(controller);
    content::WebContentsObserver::Observe(nullptr);
  }
};

class ToolbarDependencyProvider : public WebUIToolbarUI::DependencyProvider {
 public:
  ToolbarDependencyProvider() = default;
  ~ToolbarDependencyProvider() = default;

  // This might blow up in the future. We are implicitly assuming that the
  // delegate isn't going to be used in this test.
  browser_controls_api::BrowserControlsService::BrowserControlsServiceDelegate*
  GetBrowserControlsDelegate() override {
    return nullptr;
  }

  toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate*
  GetToolbarUIServiceDelegate() override {
    return nullptr;
  }

  std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher>
  GetNavigationControlsStateFetcher() override {
    return std::make_unique<toolbar_ui_api::NavigationControlsStateFetcherImpl>(
        base::BindLambdaForTesting([]() {
          return toolbar_ui_api::mojom::NavigationControlsState::New(
              toolbar_ui_api::mojom::ReloadControlState::New(),
              toolbar_ui_api::mojom::SplitTabsControlState::New(),
              /*layout_constants_version=*/0);
        }));
  }
};

class WebUIToolbarInitializer : public WebUIControllerInitalizer {
 public:
  WebUIToolbarInitializer() = default;
  ~WebUIToolbarInitializer() override = default;

  void Init(content::WebUIController* controller) override {
    auto* toolbar_controller = controller->GetAs<WebUIToolbarUI>();
    toolbar_controller->Init(&injector_);
  }

 private:
  ToolbarDependencyProvider injector_;
};

class InitialWebUINavigationBrowserTest : public InProcessBrowserTest {
 public:
  InitialWebUINavigationBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kInitialWebUI, {{"use_separate_process", "true"}}},
         {features::kWebUIReloadButton, {}},
         {features::kSkipIPCChannelPausingForNonGuests, {}},
         {features::kWebUIInProcessResourceLoadingV2, {}},
         {features::kInitialWebUISyncNavStartToCommit, {}}},
        {});
  }

 protected:
  std::unique_ptr<content::WebContents> CreateAndNavigateWebContents(
      const GURL& url,
      WebUIControllerInitalizer* initializer) {
    // Create a new WebContents, since initial WebUI navigations are only
    // allowed to happen as the first navigation in a new WebContents.
    content::BrowserContext* browser_context = browser()
                                                   ->tab_strip_model()
                                                   ->GetActiveWebContents()
                                                   ->GetBrowserContext();
    content::WebContents::CreateParams new_contents_params(
        browser_context,
        content::SiteInstance::CreateForURL(browser_context, url));
    std::unique_ptr<content::WebContents> new_web_contents(
        content::WebContents::Create(new_contents_params));
    if (initializer) {
      initializer->Watch(new_web_contents.get());
    }
    webui::SetBrowserWindowInterface(new_web_contents.get(), browser());

    // Navigate to `url`.
    content::NavigationController& controller =
        new_web_contents->GetController();
    content::TestNavigationObserver navigation_observer(url);
    navigation_observer.WatchExistingWebContents();
    auto handle = controller.LoadURLWithParams(
        content::NavigationController::LoadURLParams(url));

    navigation_observer.Wait();

    // Ensure the navigation successfully commits.
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(navigation_observer.last_navigation_url(), url);

    return new_web_contents;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Initial WebUI flag should be correctly set even if a non-initial WebUI is
// created first.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest,
                       Flag_NonInitialTopChromeCommittedFirst) {
  // 1) Navigate to non-initial WebUI topchrome in a new WebContents.
  GURL url(chrome::kChromeUITabSearchURL);
  EXPECT_TRUE(IsTopChromeWebUIURL(url));
  EXPECT_FALSE(IsForInitialWebUI(url));
  std::unique_ptr<content::WebContents> non_initial_webui_web_contents =
      CreateAndNavigateWebContents(url, nullptr);
  // Ensure that the process doesn't have the initial WebUI flag set.
  EXPECT_FALSE(non_initial_webui_web_contents->GetPrimaryMainFrame()
                   ->GetProcess()
                   ->IsForInitialWebUI());

  // 2) Navigate to initial WebUI in a new WebContents.
  GURL url2(chrome::kChromeUIWebUIToolbarURL);
  EXPECT_TRUE(IsTopChromeWebUIURL(url2));
  EXPECT_TRUE(IsForInitialWebUI(url2));
  WebUIToolbarInitializer initializer;
  std::unique_ptr<content::WebContents> initial_webui_web_contents =
      CreateAndNavigateWebContents(url2, &initializer);
  // Ensure that the process has the initial WebUI flag set.
  EXPECT_TRUE(initial_webui_web_contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsForInitialWebUI());
}

// Initial WebUI process shoudl not be shared with non-initial WebUI.
IN_PROC_BROWSER_TEST_F(InitialWebUINavigationBrowserTest,
                       InitialWebUIProcessSharing) {
  // 1) Navigate to initial WebUI in a new WebContents.
  GURL url(chrome::kChromeUIWebUIToolbarURL);
  EXPECT_TRUE(IsTopChromeWebUIURL(url));
  EXPECT_TRUE(IsForInitialWebUI(url));
  WebUIToolbarInitializer initializer;
  std::unique_ptr<content::WebContents> initial_webui_web_contents =
      CreateAndNavigateWebContents(url, &initializer);

  // Ensure that the process has the initial WebUI flag set.
  EXPECT_TRUE(initial_webui_web_contents->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsForInitialWebUI());

  // 2) Navigate to non-initial WebUI topchrome in a new WebContents.
  GURL url2(chrome::kChromeUITabSearchURL);
  EXPECT_TRUE(IsTopChromeWebUIURL(url2));
  EXPECT_FALSE(IsForInitialWebUI(url2));
  std::unique_ptr<content::WebContents> non_initial_webui_web_contents =
      CreateAndNavigateWebContents(url2, nullptr);

  // Ensure that the process doesn't have the initial WebUI flag set.
  EXPECT_FALSE(non_initial_webui_web_contents->GetPrimaryMainFrame()
                   ->GetProcess()
                   ->IsForInitialWebUI());
  // Initial WebUI and non-initial WebUI should use different processes.
  EXPECT_NE(
      initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess(),
      non_initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess());

  // 3) Navigate to initial WebUI again in a new WebContents.
  std::unique_ptr<content::WebContents> initial_webui_web_contents2 =
      CreateAndNavigateWebContents(url, &initializer);

  // Initial WebUI should share process with the other initial WebUI
  // WebContents, but not the non-initial WebUI topchrome one.
  EXPECT_EQ(initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess(),
            initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(
      initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess(),
      non_initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess());

  // 4) Navigate to another non-initial WebUI topchrome in a new WebContents.
  GURL url3(chrome::kChromeUIReadLaterURL);
  EXPECT_TRUE(IsTopChromeWebUIURL(url3));
  EXPECT_FALSE(IsForInitialWebUI(url3));
  std::unique_ptr<content::WebContents> non_initial_webui_web_contents2 =
      CreateAndNavigateWebContents(url3, nullptr);

  // Non-initial topchrome WebUI should share process with the other initial
  // WebUI WebContents, but not the initial WebUI topchrome one.
  EXPECT_EQ(
      non_initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess(),
      non_initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(
      non_initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess(),
      initial_webui_web_contents->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(
      non_initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess(),
      initial_webui_web_contents2->GetPrimaryMainFrame()->GetProcess());
}

}  // namespace

}  // namespace waap
