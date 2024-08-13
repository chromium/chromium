// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_cvc_save_message_delegate.h"

#include "base/android/jni_android.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
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
            (payments::PaymentsAutofillClient::LocalSaveCardPromptCallback)
                base::DoNothing(),
            payments::PaymentsAutofillClient::SaveCreditCardOptions(),
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

  MockAutofillSaveCardDelegateAndroid* ShowMessage(
      const AutofillSaveCardUiInfo& ui_info =
          AutofillSaveCardUiInfo::CreateForLocalSave(
              payments::PaymentsAutofillClient::SaveCreditCardOptions()
                  .with_card_save_type(payments::PaymentsAutofillClient::
                                           CardSaveType::kCvcSaveOnly),
              CreditCard())) {
    autofill_cvc_save_message_delegate_ =
        std::make_unique<AutofillCvcSaveMessageDelegate>(web_contents());
    auto mock =
        std::make_unique<MockAutofillSaveCardDelegateAndroid>(web_contents());
    auto* pointer = mock.get();
    autofill_cvc_save_message_delegate_->ShowMessage(ui_info, std::move(mock));
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

// Tests that clicking the cancel button to decline the message calls the
// callback with user decision.
TEST_F(AutofillCvcSaveMessageDelegateTest, MessageDeclined) {
  // Show the message, and save the created `MessageWrapper`.
  messages::MessageWrapper* message_wrapper;
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(DoAll(SaveArg<0>(&message_wrapper), Return(true)));
  auto* save_card_delegate = ShowMessage();

  EXPECT_CALL(*save_card_delegate, OnUiCanceled);

  // Simulate user rejection by clicking the "No thanks" button.
  message_wrapper->HandleDismissCallback(
      base::android::AttachCurrentThread(),
      static_cast<int>(messages::DismissReason::SECONDARY_ACTION));
}

// Tests that swiping to dismiss the message calls the callback with user
// decision.
TEST_F(AutofillCvcSaveMessageDelegateTest, MessageDismissedBySwiping) {
  // Show the message, and save the created `MessageWrapper`.
  messages::MessageWrapper* message_wrapper;
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(DoAll(SaveArg<0>(&message_wrapper), Return(true)));
  auto* save_card_delegate = ShowMessage();

  EXPECT_CALL(*save_card_delegate, OnUiIgnored);

  // Simulate user dismissing the prompt by swiping on the UI.
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

TEST_F(AutofillCvcSaveMessageDelegateTest, MessagePropertiesAreSet) {
  AutofillSaveCardUiInfo ui_info = AutofillSaveCardUiInfo::CreateForLocalSave(
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_card_save_type(
              payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly),
      CreditCard());

  // Show the message, and save the created `MessageWrapper`.
  messages::MessageWrapper* message_wrapper;
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(DoAll(SaveArg<0>(&message_wrapper), Return(true)));
  ShowMessage(ui_info);

  // Verify the message properties are correctly set.
  EXPECT_EQ(message_wrapper->GetTitle(), ui_info.title_text);
  EXPECT_EQ(message_wrapper->GetDescription(), ui_info.description_text);
  EXPECT_EQ(message_wrapper->GetPrimaryButtonText(), ui_info.confirm_text);
  EXPECT_EQ(message_wrapper->GetIconResourceId(),
            ResourceMapper::MapToJavaDrawableId(ui_info.logo_icon_id));
  EXPECT_EQ(message_wrapper->GetSecondaryIconResourceId(),
            ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  EXPECT_EQ(message_wrapper->GetSecondaryButtonMenuText(), ui_info.cancel_text);
}

}  // namespace autofill
