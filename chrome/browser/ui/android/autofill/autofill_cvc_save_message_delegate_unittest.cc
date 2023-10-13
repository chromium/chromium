// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_cvc_save_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"

namespace autofill {

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

class MockAutofillSaveCardDelegateAndroid
    : public AutofillSaveCardDelegateAndroid {
 public:
  explicit MockAutofillSaveCardDelegateAndroid(
      content::WebContents* web_contents)
      : AutofillSaveCardDelegateAndroid(
            (AutofillClient::LocalSaveCardPromptCallback)base::DoNothing(),
            AutofillClient::SaveCreditCardOptions(),
            web_contents) {}

  MOCK_METHOD(void, OnUiAccepted, (base::OnceClosure), (override));
  MOCK_METHOD(void, OnUiCanceled, (), (override));
  MOCK_METHOD(void, OnUiIgnored, (), (override));
};

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
  }
  void TearDown() override {
    // Reset explicitly to ensure that the destructor does not access a task
    // runner that no longer exists.
    autofill_cvc_save_message_delegate_.reset();
    messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockAutofillSaveCardDelegateAndroid* ShowMessage() {
    autofill_cvc_save_message_delegate_ =
        std::make_unique<AutofillCvcSaveMessageDelegate>(web_contents());
    auto mock =
        std::make_unique<MockAutofillSaveCardDelegateAndroid>(web_contents());
    auto* pointer = mock.get();
    autofill_cvc_save_message_delegate_->ShowMessage(std::move(mock));
    return pointer;
  }

 protected:
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;

 private:
  std::unique_ptr<AutofillCvcSaveMessageDelegate>
      autofill_cvc_save_message_delegate_;
};

// Tests that the message is shown.
TEST_F(AutofillCvcSaveMessageDelegateTest, MessageShown) {
  // Verify that the message was enqueued.
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(Return(true));

  ShowMessage();
}

// Tests that accepting the message calls the callback with user decision.
TEST_F(AutofillCvcSaveMessageDelegateTest, MessageAccepted) {
  // Show the message, and save the created `MessageWrapper`.
  messages::MessageWrapper* message_wrapper;
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(DoAll(SaveArg<0>(&message_wrapper), Return(true)));
  auto* save_card_delegate = ShowMessage();

  EXPECT_CALL(*save_card_delegate, OnUiAccepted);

  // Simulate user acceptance on the prompt.
  message_wrapper->HandleActionClick(base::android::AttachCurrentThread());
}

// Tests that declining the message calls the callback with user decision.
TEST_F(AutofillCvcSaveMessageDelegateTest, MessageDeclined) {
  // Show the message, and save the created `MessageWrapper`.
  messages::MessageWrapper* message_wrapper;
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(DoAll(SaveArg<0>(&message_wrapper), Return(true)));
  auto* save_card_delegate = ShowMessage();

  EXPECT_CALL(*save_card_delegate, OnUiCanceled);

  // Simulate user rejection on the prompt.
  message_wrapper->HandleDismissCallback(
      base::android::AttachCurrentThread(),
      static_cast<int>(messages::DismissReason::GESTURE));
}

// Tests that ignoring the message calls the callback with user decision.
TEST_F(AutofillCvcSaveMessageDelegateTest, MessageIgnored) {
  // Show the message, and save the created `MessageWrapper`.
  messages::MessageWrapper* message_wrapper;
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(DoAll(SaveArg<0>(&message_wrapper), Return(true)));
  auto* save_card_delegate = ShowMessage();

  EXPECT_CALL(*save_card_delegate, OnUiIgnored);

  // Simulate the message being ignored (auto-dismissed after a certain time).
  message_wrapper->HandleDismissCallback(
      base::android::AttachCurrentThread(),
      static_cast<int>(messages::DismissReason::TIMER));
}

}  // namespace autofill
