// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_cvc_save_message_delegate.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"

namespace autofill {

class AutofillCvcSaveMessageDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillCvcSaveMessageDelegateTest() = default;
  ~AutofillCvcSaveMessageDelegateTest() override = default;

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    messages::MessageDispatcherBridge::SetInstanceForTesting(
        &message_dispatcher_bridge_);
    autofill_cvc_save_message_delegate_ =
        std::make_unique<AutofillCvcSaveMessageDelegate>(web_contents());
  };
  void TearDown() override {
    messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  };

 protected:
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<AutofillCvcSaveMessageDelegate>
      autofill_cvc_save_message_delegate_;
};

// Tests that the message is shown.
TEST_F(AutofillCvcSaveMessageDelegateTest, MessageShown) {
  // Verify that the message was enqueued.
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(testing::Return(true));

  autofill_cvc_save_message_delegate_->ShowMessage();
}

// Tests that when 2 messages are shown back to back, the first message gets
// dismissed.
TEST_F(AutofillCvcSaveMessageDelegateTest,
       TwoMessagesShown_FirstMessageDismissed) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .Times(2)
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(message_dispatcher_bridge_,
              DismissMessage(testing::_,
                             messages::DismissReason::DISMISSED_BY_FEATURE));

  autofill_cvc_save_message_delegate_->ShowMessage();
  autofill_cvc_save_message_delegate_->ShowMessage();
}

}  // namespace autofill
