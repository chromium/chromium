// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/installable/installable_ambient_badge_message_controller.h"

#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {

namespace {
constexpr char16_t kAppName[] = u"App name";
}  // namespace

class InstallableAmbientBadgeMessageControllerTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override;
  void TearDown() override;

  void EnqueueMessage();
  void DismissMessage(bool expected);

  InstallableAmbientBadgeMessageController* message_controller() {
    return &message_controller_;
  }

  messages::MessageWrapper* message_wrapper() { return message_wrapper_; }

 private:
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  InstallableAmbientBadgeMessageController message_controller_;
  messages::MessageWrapper* message_wrapper_ = nullptr;
};

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
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(testing::DoAll(testing::SaveArg<0>(&message_wrapper_),
                               testing::Return(true)));
  message_controller_.EnqueueMessage(web_contents(), kAppName);
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
  EXPECT_FALSE(message_wrapper()->GetPrimaryButtonText().empty());

  DismissMessage(true);
}

}  // namespace webapps