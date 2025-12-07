// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/popup_opener_tab_helper.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kUkmAllowed[] = "Allowed";
}  // namespace

class PopupOpenerTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  PopupOpenerTabHelperBrowserTest() = default;
  ~PopupOpenerTabHelperBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Opens and waits for a pop-up to finish navigation before closing the
  // pop-up. Returns the opener's source id.
  void OpenAndClosePopup() {
    content::TestNavigationObserver navigation_observer(nullptr, 1);
    navigation_observer.StartWatchingNewWebContents();

    EXPECT_TRUE(
        content::ExecJs(GetActiveWebContents(), "window.open('/title1.html')"));
    navigation_observer.Wait();

    // Close the popup.
    content::WebContents* popup = GetActiveWebContents();
    int active_index = browser()->tab_strip_model()->active_index();
    content::WebContentsDestroyedWatcher destroyed_watcher(popup);
    browser()->tab_strip_model()->CloseWebContentsAt(
        active_index, TabCloseTypes::CLOSE_USER_GESTURE);
    destroyed_watcher.Wait();
  }

 protected:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(PopupOpenerTabHelperBrowserTest,
                       UserDefaultPopupBlockerSetting_MetricLoggedOnPopup) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  // Open and close two pop-ups, the opener id does not change.
  const ukm::SourceId opener_source_id =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  OpenAndClosePopup();
  OpenAndClosePopup();

  const auto& entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Popup_Page::kEntryName);

  // Only a single Popup_Page should be logged per source id.
  EXPECT_EQ(entries.size(), 1u);

  // The source id should match the opener's source id at the time of opening
  // the pop-up.
  EXPECT_EQ(entries[0]->source_id, opener_source_id);

  // Profile content settings default to pop-ups not explicitly allowed.
  test_ukm_recorder_->ExpectEntryMetric(entries[0], kUkmAllowed, 0u);
}

IN_PROC_BROWSER_TEST_F(PopupOpenerTabHelperBrowserTest,
                       UserAllowsAllPopups_MetricLoggedOnPopup) {
  const GURL first_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  host_content_settings_map->SetContentSettingDefaultScope(
      first_url, first_url, ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);

  // Open and close two pop-ups, the opener id does not change.
  const ukm::SourceId opener_source_id =
      GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  OpenAndClosePopup();
  OpenAndClosePopup();

  const auto& entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Popup_Page::kEntryName);

  // Only a single Popup_Page should be logged per source id.
  EXPECT_EQ(entries.size(), 1u);

  // The source id should match the opener's source id at the time of opening
  // the pop-up.
  EXPECT_EQ(entries[0]->source_id, opener_source_id);

  // Profile content settings default to pop-ups not explicitly allowed.
  test_ukm_recorder_->ExpectEntryMetric(entries[0], kUkmAllowed, 1u);
}
