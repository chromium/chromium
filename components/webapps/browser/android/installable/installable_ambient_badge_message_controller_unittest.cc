// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/installable/installable_ambient_badge_message_controller.h"

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_client.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace webapps {

namespace {
constexpr char16_t kAppName[] = u"App name";
}  // namespace

class MockInstallableAmbientBadgeClient : public InstallableAmbientBadgeClient {
 public:
  MockInstallableAmbientBadgeClient() = default;
  ~MockInstallableAmbientBadgeClient() override = default;

  MOCK_METHOD(void, AddToHomescreenFromBadge, (), (override));
  MOCK_METHOD(void, BadgeDismissed, (), (override));
  MOCK_METHOD(void, BadgeIgnored, (), (override));
};

class InstallableAmbientBadgeMessageControllerTest
    : public content::RenderViewHostTestHarness {
 public:
  InstallableAmbientBadgeMessageControllerTest();

  void SetUp() override;
  void TearDown() override;

  void EnqueueMessage();
  void EnqueueMessage(bool maskable);
  void EnqueueMessageWithExpectNotCalled();
  void DismissMessage(bool expected);

  void TriggerActionClick();
  void TriggerMessageDismissedWithGesture();
  void TriggerMessageDismissedWithTimer();

  InstallableAmbientBadgeMessageController* message_controller() {
    return &message_controller_;
  }

  void ExpectedIconChanged() {
    SkBitmap bitmap = message_wrapper_->GetIconBitmap();
    EXPECT_NE(bitmap.bounds(), test_icon.bounds());
  }

  void ExpectedIconUnchanged() {
    SkBitmap bitmap = message_wrapper_->GetIconBitmap();
    EXPECT_EQ(bitmap.bounds(), test_icon.bounds());
  }

  messages::MessageWrapper* message_wrapper() { return message_wrapper_; }
  MockInstallableAmbientBadgeClient& client_mock() { return client_mock_; }

 private:
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  MockInstallableAmbientBadgeClient client_mock_;
  InstallableAmbientBadgeMessageController message_controller_;
  raw_ptr<messages::MessageWrapper> message_wrapper_ = nullptr;
  SkBitmap test_icon;
};

InstallableAmbientBadgeMessageControllerTest::
    InstallableAmbientBadgeMessageControllerTest()
    : message_controller_(&client_mock_) {}

void InstallableAmbientBadgeMessageControllerTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
}

void InstallableAmbientBadgeMessageControllerTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  content::RenderViewHostTestHarness::TearDown();
}

void InstallableAmbientBadgeMessageControllerTest::EnqueueMessage() {
  EnqueueMessage(false);
}

void InstallableAmbientBadgeMessageControllerTest::EnqueueMessage(
    bool maskable) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&message_wrapper_),
                               testing::Return(true)));
  test_icon.allocPixels(SkImageInfo::Make(100, 100, kRGBA_8888_SkColorType,
                                          kUnpremul_SkAlphaType));
  message_controller_.EnqueueMessage(web_contents(), kAppName, test_icon,
                                     maskable, GURL("https://example.com/"));
}

void InstallableAmbientBadgeMessageControllerTest::
    EnqueueMessageWithExpectNotCalled() {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage).Times(0);
  test_icon.allocPixels(SkImageInfo::Make(100, 100, kRGBA_8888_SkColorType,
                                          kUnpremul_SkAlphaType));
  message_controller_.EnqueueMessage(web_contents(), kAppName, test_icon, false,
                                     GURL("https://example.com/"));
}

void InstallableAmbientBadgeMessageControllerTest::DismissMessage(
    bool bridge_dismiss_call_expected) {
  if (bridge_dismiss_call_expected) {
    EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
        .WillOnce([](messages::MessageWrapper* message,
                     messages::DismissReason dismiss_reason) {
          message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                         static_cast<int>(dismiss_reason));
        });
  }
  message_controller_.DismissMessage();
}

void InstallableAmbientBadgeMessageControllerTest::TriggerActionClick() {
  message_wrapper()->HandleActionClick(base::android::AttachCurrentThread());
  // Simulate call from Java to dismiss message on primary button click.
  message_wrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(),
      static_cast<int>(messages::DismissReason::PRIMARY_ACTION));
}

void InstallableAmbientBadgeMessageControllerTest::
    TriggerMessageDismissedWithGesture() {
  message_wrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(),
      static_cast<int>(messages::DismissReason::GESTURE));
}

void InstallableAmbientBadgeMessageControllerTest::
    TriggerMessageDismissedWithTimer() {
  message_wrapper()->HandleDismissCallback(
      base::android::AttachCurrentThread(),
      static_cast<int>(messages::DismissReason::TIMER));
}

// Tests InstallableAmbientBadgeMessageController API: EnqueueMessage,
// IsMessageEnqueued, DismissMessage.
TEST_F(InstallableAmbientBadgeMessageControllerTest, APITest) {
  ASSERT_FALSE(message_controller()->IsMessageEnqueued());
  EnqueueMessage();
  ASSERT_TRUE(message_controller()->IsMessageEnqueued());
  DismissMessage(true);
  ASSERT_FALSE(message_controller()->IsMessageEnqueued());
  // Calling DismissMessage when there is no message enqueued should not fail or
  // result in a call to MessageDispatcherBridge.
  DismissMessage(false);
}

// Tests that message properties are set correctly.
TEST_F(InstallableAmbientBadgeMessageControllerTest, MessagePropertyValues) {
  EnqueueMessage();

  EXPECT_NE(std::u16string::npos, message_wrapper()->GetTitle().find(kAppName));
  EXPECT_FALSE(message_wrapper()->GetDescription().empty());
  EXPECT_FALSE(message_wrapper()->GetPrimaryButtonText().empty());
  EXPECT_TRUE(message_wrapper()->IsValidIcon());

  DismissMessage(true);
}

// Tests that when the user taps on Install, client's AddToHomescreenFromBadge
// method is called.
TEST_F(InstallableAmbientBadgeMessageControllerTest, AddToHomeSceen) {
  EnqueueMessage();
  EXPECT_CALL(client_mock(), AddToHomescreenFromBadge);
  EXPECT_CALL(client_mock(), BadgeDismissed).Times(0);
  EXPECT_CALL(client_mock(), BadgeIgnored).Times(0);
  ExpectedIconUnchanged();
  TriggerActionClick();
}

// Tests that message is using an adaptive icon when icon is maskable.
TEST_F(InstallableAmbientBadgeMessageControllerTest, MaskableIcon) {
  EnqueueMessage(true);
  EXPECT_CALL(client_mock(), AddToHomescreenFromBadge);
  EXPECT_CALL(client_mock(), BadgeDismissed).Times(0);
  EXPECT_CALL(client_mock(), BadgeIgnored).Times(0);
  if (WebappsIconUtils::DoesAndroidSupportMaskableIcons()) {
    ExpectedIconChanged();
  } else {
    ExpectedIconUnchanged();
  }
  TriggerActionClick();
}

// Tests that when the user dismisses the message with a gesture, client's
// BadgeDismissed method is called.
TEST_F(InstallableAmbientBadgeMessageControllerTest, Dismiss) {
  EnqueueMessage();
  EXPECT_CALL(client_mock(), AddToHomescreenFromBadge).Times(0);
  EXPECT_CALL(client_mock(), BadgeDismissed);
  TriggerMessageDismissedWithGesture();
}

// Tests that when the user dismisses the message with a gesture, client's
// BadgeDismissed method is called and the message is not enqueued
// because of throttling.
TEST_F(InstallableAmbientBadgeMessageControllerTest, Throttle) {
  EnqueueMessage();
  EXPECT_CALL(client_mock(), AddToHomescreenFromBadge).Times(0);
  DismissMessage(true);
  EnqueueMessageWithExpectNotCalled();
  ASSERT_FALSE(message_controller()->IsMessageEnqueued());
}

// Tests that when the message is dismissed with the timer, client's
// BadgeIgnored method is called.
TEST_F(InstallableAmbientBadgeMessageControllerTest, TimerDismissed) {
  EnqueueMessage();
  EXPECT_CALL(client_mock(), AddToHomescreenFromBadge).Times(0);
  EXPECT_CALL(client_mock(), BadgeDismissed);
  TriggerMessageDismissedWithGesture();
}

}  // namespace webapps
