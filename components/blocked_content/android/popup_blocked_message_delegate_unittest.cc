// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/android/popup_blocked_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace blocked_content {
namespace {
constexpr char kPageUrl[] = "http://example_page.test";
}  // namespace

class PopupBlockedMessageDelegateTest
    : public content::RenderViewHostTestHarness {
 public:
  PopupBlockedMessageDelegateTest() = default;
  ~PopupBlockedMessageDelegateTest() override;

  // content::RenderViewHostTestHarness:
  void SetUp() override;

  PopupBlockerTabHelper* helper() { return helper_; }

  HostContentSettingsMap* settings_map() { return settings_map_.get(); }

  bool EnqueueMessage(int num_pops,
                      base::OnceClosure on_accept_callback,
                      bool success);

  messages::MessageWrapper* GetMessageWrapper();
  void TriggerMessageDismissedCallback(messages::DismissReason dismiss_reason);
  void TriggerActionClick();

  PopupBlockedMessageDelegate* GetDelegate() {
    return popup_blocked_message_delegate_;
  }

 private:
  raw_ptr<PopupBlockerTabHelper> helper_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  raw_ptr<PopupBlockedMessageDelegate> popup_blocked_message_delegate_;
};

PopupBlockedMessageDelegateTest::~PopupBlockedMessageDelegateTest() {
  settings_map_->ShutdownOnUIThread();
}

void PopupBlockedMessageDelegateTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();

  // Make sure the SafeBrowsingTriggeredPopupBlocker is not created.
  feature_list_.InitAndDisableFeature(kAbusiveExperienceEnforce);

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

  PopupBlockedMessageDelegate::CreateForWebContents(web_contents());
  popup_blocked_message_delegate_ =
      PopupBlockedMessageDelegate::FromWebContents(web_contents());
  NavigateAndCommit(GURL(kPageUrl));
  message_dispatcher_bridge_.SetMessagesEnabledForEmbedder(true);
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

messages::MessageWrapper* PopupBlockedMessageDelegateTest::GetMessageWrapper() {
  return popup_blocked_message_delegate_->message_for_testing();
}

bool PopupBlockedMessageDelegateTest::EnqueueMessage(
    int num_pops,
    base::OnceClosure on_accept_callback,
    bool success) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(testing::Return(success));
  return GetDelegate()->ShowMessage(num_pops, settings_map(),
                                    std::move(on_accept_callback));
}

void PopupBlockedMessageDelegateTest::TriggerActionClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

void PopupBlockedMessageDelegateTest::TriggerMessageDismissedCallback(
    messages::DismissReason dismiss_reason) {
  GetMessageWrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(), static_cast<int>(dismiss_reason));
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// Tests that message properties (title, description, icon, button text) are
// set correctly.
TEST_F(PopupBlockedMessageDelegateTest, MessagePropertyValues) {
  int num_popups = 3;
  EnqueueMessage(num_popups, base::NullCallback(), true);
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_POPUPS_BLOCKED_INFOBAR_TEXT,
                                             num_popups),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_POPUPS_BLOCKED_INFOBAR_BUTTON_SHOW),
            GetMessageWrapper()->GetPrimaryButtonText());

  // Should update title; #EnqueueMessage ensure message is enqueued only once.
  GetDelegate()->ShowMessage(num_popups + 1, settings_map(),
                             base::NullCallback());
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(IDS_POPUPS_BLOCKED_INFOBAR_TEXT,
                                             num_popups + 1),
            GetMessageWrapper()->GetTitle());
  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
}

// Tests that title updated when another popup is blocked and a message
// is already on the screen.
TEST_F(PopupBlockedMessageDelegateTest, ShowsBlockedPopups) {
  bool on_accept_called = false;
  bool result =
      EnqueueMessage(1, base::BindLambdaForTesting([&on_accept_called] {
                       on_accept_called = true;
                     }),
                     true);
  EXPECT_TRUE(result);
  TriggerActionClick();
  EXPECT_TRUE(on_accept_called);
  TriggerMessageDismissedCallback(messages::DismissReason::UNKNOWN);
  EXPECT_EQ(settings_map()->GetContentSetting(GURL(kPageUrl), GURL(kPageUrl),
                                              ContentSettingsType::POPUPS),
            CONTENT_SETTING_ALLOW);
}

// Tests that title updated when another popup is blocked and a message
// is already on the screen.
TEST_F(PopupBlockedMessageDelegateTest, FailToShowMessage) {
  bool on_accept_called = false;
  bool result =
      EnqueueMessage(1, base::BindLambdaForTesting([&on_accept_called] {
                       on_accept_called = true;
                     }),
                     false);
  EXPECT_FALSE(result);
  EXPECT_FALSE(on_accept_called);
}

}  // namespace blocked_content
