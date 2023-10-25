// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/popup_blocker_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "components/blocked_content/popup_navigation_delegate.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/blocked_content/test/test_popup_navigation_delegate.h"
#include "components/blocked_content/url_list_manager.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace blocked_content {
namespace {
using testing::Pair;
using testing::UnorderedElementsAre;

constexpr char kUrl1[] = "http://example1.test";
constexpr char kUrl2[] = "http://example2.test";

// Observer which allows retrieving a map of all the blocked URLs.
class BlockedUrlListObserver : public UrlListManager::Observer {
 public:
  explicit BlockedUrlListObserver(PopupBlockerTabHelper* helper) {
    observation_.Observe(helper->manager());
  }
  // UrlListManager::Observer:
  void BlockedUrlAdded(int32_t id, const GURL& url) override {
    blocked_urls_.insert({id, url});
  }

  const std::map<int32_t, GURL>& blocked_urls() const { return blocked_urls_; }

 private:
  std::map<int32_t, GURL> blocked_urls_;
  base::ScopedObservation<UrlListManager, UrlListManager::Observer>
      observation_{this};
};
}  // namespace

class PopupBlockerTabHelperTest : public content::RenderViewHostTestHarness {
 public:
  PopupBlockerTabHelperTest() {
    // Make sure the SafeBrowsingTriggeredPopupBlocker is not created.
    // This needs to be done as early as possible to avoid tsan data races
    // caused by other threads trying to access the feature list.
    feature_list_.InitAndDisableFeature(kAbusiveExperienceEnforce);
  }
  ~PopupBlockerTabHelperTest() override { settings_map_->ShutdownOnUIThread(); }

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    HostContentSettingsMap::RegisterProfilePrefs(pref_service_.registry());
    settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &pref_service_, false /* is_off_the_record */,
        false /* store_last_modified */, false /* restore_session*/,
        false /* should_record_metrics */);
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<
            content_settings::TestPageSpecificContentSettingsDelegate>(
            /*prefs=*/nullptr, settings_map_.get()));

    PopupBlockerTabHelper::CreateForWebContents(web_contents());
    helper_ = PopupBlockerTabHelper::FromWebContents(web_contents());
  }
  void TearDown() override {
    helper_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  PopupBlockerTabHelper* helper() { return helper_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<PopupBlockerTabHelper> helper_ = nullptr;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
};

TEST_F(PopupBlockerTabHelperTest, BlocksAndShowsPopup) {
  BlockedUrlListObserver observer(helper());
  TestPopupNavigationDelegate::ResultHolder result;
  blink::mojom::WindowFeatures window_features;
  window_features.has_x = true;
  helper()->AddBlockedPopup(
      std::make_unique<TestPopupNavigationDelegate>(GURL(kUrl1), &result),
      window_features, PopupBlockType::kNoGesture);
  EXPECT_EQ(result.total_popups_blocked_on_page, 1);
  EXPECT_FALSE(result.did_navigate);
  EXPECT_THAT(observer.blocked_urls(),
              UnorderedElementsAre(Pair(0, GURL(kUrl1))));

  helper()->ShowBlockedPopup(0, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_TRUE(result.did_navigate);
  EXPECT_TRUE(result.navigation_window_features.has_x);
  EXPECT_EQ(result.navigation_disposition,
            WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

TEST_F(PopupBlockerTabHelperTest, MultiplePopups) {
  BlockedUrlListObserver observer(helper());
  TestPopupNavigationDelegate::ResultHolder result1;
  helper()->AddBlockedPopup(
      std::make_unique<TestPopupNavigationDelegate>(GURL(kUrl1), &result1),
      blink::mojom::WindowFeatures(), PopupBlockType::kNoGesture);
  EXPECT_EQ(result1.total_popups_blocked_on_page, 1);
  EXPECT_THAT(observer.blocked_urls(),
              UnorderedElementsAre(Pair(0, GURL(kUrl1))));
  EXPECT_EQ(helper()->GetBlockedPopupsCount(), 1u);

  TestPopupNavigationDelegate::ResultHolder result2;
  helper()->AddBlockedPopup(
      std::make_unique<TestPopupNavigationDelegate>(GURL(kUrl2), &result2),
      blink::mojom::WindowFeatures(), PopupBlockType::kNoGesture);
  EXPECT_EQ(result2.total_popups_blocked_on_page, 2);
  EXPECT_THAT(observer.blocked_urls(),
              UnorderedElementsAre(Pair(0, GURL(kUrl1)), Pair(1, GURL(kUrl2))));
  EXPECT_EQ(helper()->GetBlockedPopupsCount(), 2u);

  helper()->ShowBlockedPopup(1, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(helper()->GetBlockedPopupsCount(), 1u);
  EXPECT_TRUE(result2.did_navigate);
  EXPECT_EQ(result2.navigation_disposition,
            WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_FALSE(result1.did_navigate);

  helper()->ShowBlockedPopup(0, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_EQ(helper()->GetBlockedPopupsCount(), 0u);
  EXPECT_TRUE(result1.did_navigate);
  EXPECT_FALSE(result1.navigation_disposition.has_value());
}

TEST_F(PopupBlockerTabHelperTest, DoesNotShowPopupWithInvalidID) {
  TestPopupNavigationDelegate::ResultHolder result;
  helper()->AddBlockedPopup(
      std::make_unique<TestPopupNavigationDelegate>(GURL(kUrl1), &result),
      blink::mojom::WindowFeatures(), PopupBlockType::kNoGesture);
  EXPECT_EQ(helper()->GetBlockedPopupsCount(), 1u);

  // Invalid ID should not do anything.
  helper()->ShowBlockedPopup(1, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(helper()->GetBlockedPopupsCount(), 1u);
  EXPECT_FALSE(result.did_navigate);

  helper()->ShowBlockedPopup(0, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(helper()->GetBlockedPopupsCount(), 0u);
  EXPECT_TRUE(result.did_navigate);
}

TEST_F(PopupBlockerTabHelperTest, SetsContentSettingsPopupState) {
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));

  TestPopupNavigationDelegate::ResultHolder result;
  helper()->AddBlockedPopup(
      std::make_unique<TestPopupNavigationDelegate>(GURL(kUrl1), &result),
      blink::mojom::WindowFeatures(), PopupBlockType::kNoGesture);
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));

  helper()->AddBlockedPopup(
      std::make_unique<TestPopupNavigationDelegate>(GURL(kUrl2), &result),
      blink::mojom::WindowFeatures(), PopupBlockType::kNoGesture);
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));

  helper()->ShowBlockedPopup(0, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));

  helper()->ShowBlockedPopup(1, WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
}

TEST_F(PopupBlockerTabHelperTest, ClearsContentSettingsPopupStateOnNavigation) {
  TestPopupNavigationDelegate::ResultHolder result;
  helper()->AddBlockedPopup(
      std::make_unique<TestPopupNavigationDelegate>(GURL(kUrl1), &result),
      blink::mojom::WindowFeatures(), PopupBlockType::kNoGesture);
  EXPECT_TRUE(content_settings::PageSpecificContentSettings::GetForFrame(
                  web_contents()->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::POPUPS));

  NavigateAndCommit(GURL(kUrl2));
  EXPECT_FALSE(content_settings::PageSpecificContentSettings::GetForFrame(
                   web_contents()->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
}

TEST_F(PopupBlockerTabHelperTest,
       NavigatingNonPrimaryDoesntClearsContentSettings) {
  TestPopupNavigationDelegate::ResultHolder result;
  helper()->AddBlockedPopup(
      std::make_unique<TestPopupNavigationDelegate>(GURL(kUrl1), &result),
      blink::mojom::WindowFeatures(), PopupBlockType::kNoGesture);
  EXPECT_TRUE(content_settings::PageSpecificContentSettings::GetForFrame(
                  web_contents()->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::POPUPS));

  // Navigating a non-primary main frame shoudn't clear the popups.
  content::MockNavigationHandle handle(GURL(kUrl2),
                                       web_contents()->GetPrimaryMainFrame());
  handle.set_has_committed(true);
  handle.set_is_in_primary_main_frame(false);
  helper()->DidFinishNavigation(&handle);
  EXPECT_TRUE(content_settings::PageSpecificContentSettings::GetForFrame(
                  web_contents()->GetPrimaryMainFrame())
                  ->IsContentBlocked(ContentSettingsType::POPUPS));

  // Navigating the primary main frame should clear the popups.
  handle.set_is_in_primary_main_frame(true);
  helper()->DidFinishNavigation(&handle);
  EXPECT_FALSE(content_settings::PageSpecificContentSettings::GetForFrame(
                   web_contents()->GetPrimaryMainFrame())
                   ->IsContentBlocked(ContentSettingsType::POPUPS));
}

}  // namespace blocked_content
