// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"

#include <map>

#include "base/containers/span.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/top_chrome/preload_candidate_selector.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

namespace {

using PreloadModeName = const char*;

int GetCommandIdForURL(GURL webui_url) {
  static const std::map<GURL, int> url_to_command_id = {
      {GURL(chrome::kChromeUITabSearchURL), IDC_TAB_SEARCH},
      {GURL(chrome::kChromeUIHistoryClustersSidePanelURL),
       IDC_SHOW_BOOKMARK_SIDE_PANEL},
      {GURL(chrome::kChromeUIBookmarksSidePanelURL),
       IDC_SHOW_HISTORY_CLUSTERS_SIDE_PANEL}};

  CHECK(url_to_command_id.contains(webui_url));

  return url_to_command_id.at(webui_url);
}

std::vector<GURL> GetAllPreloadableWebUIURLs() {
  return WebUIContentsPreloadManager::GetAllPreloadableWebUIURLsForTesting();
}

std::vector<PreloadModeName> GetAllPreloadManagerModes() {
  return {features::kPreloadTopChromeWebUIModePreloadOnWarmupName,
          features::kPreloadTopChromeWebUIModePreloadOnMakeContentsName};
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

class WebUIContentsPreloadManagerBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<std::tuple<GURL, PreloadModeName>> {
 public:
  struct PrintParams {
    template <typename ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      // Remove ".top-chrome" from URL. Replace "-." with "_" since gtest does
      // not allow them in test name.
      GURL webui_url = std::get<0>(info.param);
      std::string host = webui_url.host();
      std::string preload_mode = std::get<1>(info.param);
      base::ReplaceSubstringsAfterOffset(&host, 0, ".top-chrome", "");
      base::ReplaceChars(host, "-.", "_", &host);
      base::ReplaceChars(preload_mode, "-.", "_", &preload_mode);
      return host + "_" + preload_mode;
    }
  };

  // InProcessBrowserTest:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPreloadTopChromeWebUI,
        {{features::kPreloadTopChromeWebUIModeName, std::get<1>(GetParam())}});

    // Always preload the WebUI specified by the test param.
    auto preload_candidate_selector =
        std::make_unique<testing::NiceMock<MockPreloadCandidateSelector>>();
    preload_candidate_selector_ = preload_candidate_selector.get();
    preload_manager()->SetPreloadCandidateSelectorForTesting(
        std::move(preload_candidate_selector));
    ON_CALL(*preload_candidate_selector_, GetURLToPreload(_))
        .WillByDefault(Return(std::get<0>(GetParam())));

    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    navigation_waiter_ = std::make_unique<content::TestNavigationObserver>(
        preload_manager()->preloaded_web_contents());
    navigation_waiter_->StartWatchingNewWebContents();

    InProcessBrowserTest::SetUpOnMainThread();
  }
  void TearDown() override {
    preload_candidate_selector_ = nullptr;
    // The mock object does not expect itself to leak outside of the test.
    // Clearing it from the preload manager to destroy it.
    preload_manager()->SetPreloadCandidateSelectorForTesting(nullptr);

    InProcessBrowserTest::TearDown();
  }

  WebUIContentsPreloadManager* preload_manager() {
    return WebUIContentsPreloadManager::GetInstance();
  }

  content::TestNavigationObserver* navigation_waiter() {
    return navigation_waiter_.get();
  }

 private:
  std::unique_ptr<content::TestNavigationObserver> navigation_waiter_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockPreloadCandidateSelector> preload_candidate_selector_;
};

// A smoke test that ensures the browser does not crash when triggering
// a preloaded WebUI.
IN_PROC_BROWSER_TEST_P(WebUIContentsPreloadManagerBrowserTest,
                       TriggerPreloadedUI) {
  const GURL webui_url = std::get<0>(GetParam());
  const std::string preload_mode = std::get<1>(GetParam());
  if (preload_mode == features::kPreloadTopChromeWebUIModePreloadOnWarmupName) {
    ASSERT_TRUE(preload_manager()->preloaded_web_contents());
    ASSERT_EQ(preload_manager()->preloaded_web_contents()->GetVisibleURL(),
              webui_url);
  }
  browser()->command_controller()->ExecuteCommand(
      GetCommandIdForURL(webui_url));
  navigation_waiter()->Wait();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebUIContentsPreloadManagerBrowserTest,
    testing::Combine(testing::ValuesIn(GetAllPreloadableWebUIURLs()),
                     testing::ValuesIn(GetAllPreloadManagerModes())),
    WebUIContentsPreloadManagerBrowserTest::PrintParams());
