// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/android/popup_blocked_infobar_delegate.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/blocked_content/test/test_popup_navigation_delegate.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blocked_content {
namespace {

constexpr char kPageUrl[] = "http://example_page.test";
constexpr char kPopupUrl[] = "http://example_popup.test";

}  // namespace

class PopupBlockedInfoBarDelegateTest
    : public content::RenderViewHostTestHarness {
 public:
  PopupBlockedInfoBarDelegateTest() : content::RenderViewHostTestHarness() {
    // Make sure the SafeBrowsingTriggeredPopupBlocker is not created.
    feature_list_.InitAndDisableFeature(kAbusiveExperienceEnforce);
  }

  ~PopupBlockedInfoBarDelegateTest() override {
    settings_map_->ShutdownOnUIThread();
  }

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
    infobar_manager_ =
        std::make_unique<infobars::ContentInfoBarManager>(web_contents());

    NavigateAndCommit(GURL(kPageUrl));
  }

  PopupBlockerTabHelper* helper() { return helper_; }

  infobars::ContentInfoBarManager* infobar_manager() {
    return infobar_manager_.get();
  }

  HostContentSettingsMap* settings_map() { return settings_map_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<PopupBlockerTabHelper> helper_ = nullptr;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  std::unique_ptr<infobars::ContentInfoBarManager> infobar_manager_;
};

TEST_F(PopupBlockedInfoBarDelegateTest, ReplacesInfobarOnSecondPopup) {
  EXPECT_TRUE(PopupBlockedInfoBarDelegate::Create(
      infobar_manager(), 1, settings_map(), base::NullCallback()));
  EXPECT_EQ(infobar_manager()->infobars().size(), 1u);
  // First message should not contain "2";
  EXPECT_FALSE(base::Contains(infobar_manager()
                                  ->infobars()[0]
                                  ->delegate()
                                  ->AsConfirmInfoBarDelegate()
                                  ->GetMessageText(),
                              u"2"));

  EXPECT_FALSE(PopupBlockedInfoBarDelegate::Create(
      infobar_manager(), 2, settings_map(), base::NullCallback()));
  EXPECT_EQ(infobar_manager()->infobars().size(), 1u);
  // Second message blocks 2 popups, so should contain "2";
  EXPECT_TRUE(base::Contains(infobar_manager()
                                 ->infobars()[0]
                                 ->delegate()
                                 ->AsConfirmInfoBarDelegate()
                                 ->GetMessageText(),
                             u"2"));
}

TEST_F(PopupBlockedInfoBarDelegateTest, ShowsBlockedPopups) {
  TestPopupNavigationDelegate::ResultHolder result;
  helper()->AddBlockedPopup(
      std::make_unique<TestPopupNavigationDelegate>(GURL(kPopupUrl), &result),
      blink::mojom::WindowFeatures(), PopupBlockType::kNoGesture);
  bool on_accept_called = false;
  EXPECT_TRUE(PopupBlockedInfoBarDelegate::Create(
      infobar_manager(), 1, settings_map(),
      base::BindLambdaForTesting(
          [&on_accept_called] { on_accept_called = true; })));
  EXPECT_FALSE(on_accept_called);

  EXPECT_TRUE(infobar_manager()
                  ->infobars()[0]
                  ->delegate()
                  ->AsConfirmInfoBarDelegate()
                  ->Accept());
  EXPECT_TRUE(result.did_navigate);
  EXPECT_TRUE(on_accept_called);
  EXPECT_EQ(settings_map()->GetContentSetting(GURL(kPageUrl), GURL(kPageUrl),
                                              ContentSettingsType::POPUPS),
            CONTENT_SETTING_ALLOW);
}

}  // namespace blocked_content
