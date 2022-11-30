// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ads_blocked_message_delegate.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace subresource_filter {
namespace {
constexpr char kPageUrl[] = "http://example.com";
static const char kSubresourceFilterActionMetric[] =
    "SubresourceFilter.Actions2";
}  // namespace

class MockAdsBlockedDialog : public AdsBlockedDialogBase {
 public:
  MOCK_METHOD(void, Show, (bool should_post_dialog), (override));
  MOCK_METHOD(void, Dismiss, (), (override));
  MOCK_METHOD(void, Destroy, ());
  ~MockAdsBlockedDialog() override { Destroy(); }
};

class AdsBlockedMessageDelegateTest
    : public content::RenderViewHostTestHarness {
 public:
  AdsBlockedMessageDelegateTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  messages::MessageWrapper* GetMessageWrapper();

  void EnqueueMessage();

  void TriggerMessageOkClicked();
  void TriggerMessageManageClicked();
  void TriggerMessageDismissed(messages::DismissReason dismiss_reason);
  void ExpectDismissMessageCall();

  // Ads blocked dialog factory function that is passed to
  // AdsBlockedMessageDelegate. Passes the dialog prepared by
  // PrepareAdsBlockedDialog. Captures accept and dismiss callbacks.
  std::unique_ptr<AdsBlockedDialogBase> CreateAdsBlockedDialog(
      content::WebContents* web_contents,
      base::OnceClosure allow_ads_clicked_callback,
      base::OnceClosure learn_more_clicked_callback,
      base::OnceClosure dialog_dismissed_callback);

  // Creates a mock of AdsBlockedDialogBase that will be passed to
  // AdsBlockedMessageDelegate through CreateAdsBlockedDialog factory.
  // Returns non-owning pointer to the mock for test to configure mock
  // expectations.
  MockAdsBlockedDialog* PrepareAdsBlockedDialog();

  void TriggerAllowAdsClickedCallback();
  void TriggerLearnMoreClickedCallback();
  void TriggerDialogDismissedCallback();

  AdsBlockedMessageDelegate* GetDelegate() {
    return ads_blocked_message_delegate_;
  }

  messages::MockMessageDispatcherBridge* message_dispatcher_bridge() {
    return &message_dispatcher_bridge_;
  }

  void OnWebContentsFocused() {
    ads_blocked_message_delegate_->OnWebContentsFocused(nullptr);
  }

 private:
  raw_ptr<AdsBlockedMessageDelegate> ads_blocked_message_delegate_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<MockAdsBlockedDialog> mock_ads_blocked_dialog_;
  base::OnceClosure allow_ads_clicked_callback_;
  base::OnceClosure learn_more_clicked_callback_;
  base::OnceClosure dialog_dismissed_callback_;
};

void AdsBlockedMessageDelegateTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();

  AdsBlockedMessageDelegate::CreateForWebContents(
      web_contents(),
      base::BindRepeating(
          &AdsBlockedMessageDelegateTest::CreateAdsBlockedDialog,
          base::Unretained(this)));
  ads_blocked_message_delegate_ =
      AdsBlockedMessageDelegate::FromWebContents(web_contents());
  NavigateAndCommit(GURL(kPageUrl));
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void AdsBlockedMessageDelegateTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  content::RenderViewHostTestHarness::TearDown();
}

messages::MessageWrapper* AdsBlockedMessageDelegateTest::GetMessageWrapper() {
  return ads_blocked_message_delegate_->message_for_testing();
}

void AdsBlockedMessageDelegateTest::EnqueueMessage() {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  GetDelegate()->ShowMessage();
}

void AdsBlockedMessageDelegateTest::TriggerMessageOkClicked() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
  // Simulate call from Java to dismiss message on primary button click.
  GetDelegate()->DismissMessage(messages::DismissReason::PRIMARY_ACTION);
}

void AdsBlockedMessageDelegateTest::TriggerMessageManageClicked() {
  GetMessageWrapper()->HandleSecondaryActionClick(
      base::android::AttachCurrentThread());
}

void AdsBlockedMessageDelegateTest::TriggerMessageDismissed(
    messages::DismissReason dismiss_reason) {
  GetMessageWrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(), static_cast<int>(dismiss_reason));
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

void AdsBlockedMessageDelegateTest::ExpectDismissMessageCall() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
}

std::unique_ptr<AdsBlockedDialogBase>
AdsBlockedMessageDelegateTest::CreateAdsBlockedDialog(
    content::WebContents* web_contents,
    base::OnceClosure allow_ads_clicked_callback,
    base::OnceClosure learn_more_clicked_callback,
    base::OnceClosure dialog_dismissed_callback) {
  allow_ads_clicked_callback_ = std::move(allow_ads_clicked_callback);
  learn_more_clicked_callback_ = std::move(learn_more_clicked_callback);
  dialog_dismissed_callback_ = std::move(dialog_dismissed_callback);
  // PrepareAdsBlockedDialog() should always be invoked before the dialog is
  // constructed.
  EXPECT_TRUE(mock_ads_blocked_dialog_);
  return std::move(mock_ads_blocked_dialog_);
}

MockAdsBlockedDialog* AdsBlockedMessageDelegateTest::PrepareAdsBlockedDialog() {
  mock_ads_blocked_dialog_ = std::make_unique<MockAdsBlockedDialog>();
  return mock_ads_blocked_dialog_.get();
}

void AdsBlockedMessageDelegateTest::TriggerAllowAdsClickedCallback() {
  std::move(allow_ads_clicked_callback_).Run();
}

void AdsBlockedMessageDelegateTest::TriggerLearnMoreClickedCallback() {
  std::move(learn_more_clicked_callback_).Run();
}

void AdsBlockedMessageDelegateTest::TriggerDialogDismissedCallback() {
  std::move(dialog_dismissed_callback_).Run();
}

// Tests that message properties (title, icons, button text) are set correctly.
TEST_F(AdsBlockedMessageDelegateTest, MessagePropertyValues) {
  EnqueueMessage();

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_MESSAGE_PRIMARY_TEXT),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OK),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(message_dispatcher_bridge()->MapToJavaDrawableId(
                IDR_ANDROID_INFOBAR_BLOCKED_POPUPS),
            GetMessageWrapper()->GetIconResourceId());
  EXPECT_EQ(
      message_dispatcher_bridge()->MapToJavaDrawableId(IDR_ANDROID_SETTINGS),
      GetMessageWrapper()->GetSecondaryIconResourceId());

  TriggerMessageDismissed(messages::DismissReason::UNKNOWN);
}

// Tests that the message is dismissed when the 'OK' button is clicked.
TEST_F(AdsBlockedMessageDelegateTest, MessageDismissed_OnOkClicked) {
  EnqueueMessage();
  ExpectDismissMessageCall();
  TriggerMessageOkClicked();
  EXPECT_EQ(GetMessageWrapper(), nullptr);
}

// Tests that the message is dismissed, the dialog is shown and metrics
// are recorded when the gear icon is clicked.
TEST_F(AdsBlockedMessageDelegateTest, DialogTriggered_OnManageClicked) {
  base::HistogramTester histogram_tester;

  EnqueueMessage();

  ExpectDismissMessageCall();
  MockAdsBlockedDialog* mock_dialog = PrepareAdsBlockedDialog();
  EXPECT_CALL(*mock_dialog, Show(false));
  TriggerMessageManageClicked();
  EXPECT_EQ(GetMessageWrapper(), nullptr);

  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionMetric,
      subresource_filter::SubresourceFilterAction::kDetailsShown, 1);
}

// Tests that metrics are recorded when 'Learn more' is clicked on the dialog.
TEST_F(AdsBlockedMessageDelegateTest, MetricsRecorded_OnLearnMoreClicked) {
  base::HistogramTester histogram_tester;

  EnqueueMessage();

  ExpectDismissMessageCall();
  MockAdsBlockedDialog* mock_dialog = PrepareAdsBlockedDialog();
  EXPECT_CALL(*mock_dialog, Show(false));
  TriggerMessageManageClicked();
  EXPECT_EQ(GetMessageWrapper(), nullptr);

  TriggerLearnMoreClickedCallback();
  // Simulate the #onDismiss call from Java to dismiss the dialog.
  TriggerDialogDismissedCallback();

  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionMetric,
      subresource_filter::SubresourceFilterAction::kClickedLearnMore, 1);
}

// Tests that the dialog restoration state is recorded when 'Learn more' is
// clicked on the dialog.
TEST_F(AdsBlockedMessageDelegateTest, RestoreDialog_OnLearnMoreClicked) {
  EnqueueMessage();

  ExpectDismissMessageCall();
  MockAdsBlockedDialog* mock_dialog = PrepareAdsBlockedDialog();
  EXPECT_CALL(*mock_dialog, Show(false));
  TriggerMessageManageClicked();
  EXPECT_EQ(GetMessageWrapper(), nullptr);
  EXPECT_FALSE(GetDelegate()->reprompt_required_flag_for_testing());

  TriggerLearnMoreClickedCallback();
  // Simulate the #onDismiss call from Java to dismiss the dialog.
  TriggerDialogDismissedCallback();
  EXPECT_TRUE(GetDelegate()->reprompt_required_flag_for_testing());

  // Prepare the dialog to be re-shown on navigation back to the original tab.
  mock_dialog = PrepareAdsBlockedDialog();
  EXPECT_CALL(*mock_dialog, Show(true));
  OnWebContentsFocused();
  EXPECT_FALSE(GetDelegate()->reprompt_required_flag_for_testing());
}

// Tests that the AdsBlockedDialog destructor is invoked when the
// AdsBlockedMessageDelegate is destroyed.
TEST_F(AdsBlockedMessageDelegateTest, DismissDialog_OnDelegateDestroyed) {
  EnqueueMessage();

  ExpectDismissMessageCall();
  MockAdsBlockedDialog* mock_dialog = PrepareAdsBlockedDialog();
  EXPECT_CALL(*mock_dialog, Show(false));
  TriggerMessageManageClicked();

  // Verify that the AdsBlockedDialog destructor is invoked when the
  // AdsBlockedMessageDelegate is destroyed.
  EXPECT_CALL(*mock_dialog, Destroy());
  web_contents()->RemoveUserData(AdsBlockedMessageDelegate::UserDataKey());
}

}  // namespace subresource_filter
