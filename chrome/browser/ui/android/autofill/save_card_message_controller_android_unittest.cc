// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_card_message_controller_android.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace autofill {

namespace {
constexpr char16_t kDefaultUrl[] = u"http://example.com";
}  // namespace

class SaveCardMessageControllerAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SaveCardMessageControllerAndroidTest() = default;
  ~SaveCardMessageControllerAndroidTest() override = default;

 protected:
  void SetUp() override;
  void TearDown() override;

  void EnqueueMessage(AutofillClient::UploadSaveCardPromptCallback
                          upload_save_card_prompt_callback,
                      AutofillClient::LocalSaveCardPromptCallback
                          local_save_card_prompt_callback);

  void DismissMessage();

  void TriggerPrimaryButtonClick();

  messages::MessageWrapper* GetMessageWrapper() {
    return controller_.message_.get();
  }

 private:
  autofill::SaveCardMessageControllerAndroid controller_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
  std::unique_ptr<TestPersonalDataManager> personal_data_;
};

void SaveCardMessageControllerAndroidTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
      profile(), BrowserContextKeyedServiceFactory::TestingFactory());

  personal_data_ = std::make_unique<TestPersonalDataManager>();
  personal_data_->SetPrefService(profile()->GetPrefs());

  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillAcceptSaveCreditCardPromptState,
      prefs::PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE);
  NavigateAndCommit(GURL(kDefaultUrl));
}

void SaveCardMessageControllerAndroidTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  personal_data_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

void SaveCardMessageControllerAndroidTest::EnqueueMessage(
    AutofillClient::UploadSaveCardPromptCallback
        upload_save_card_prompt_callback,
    AutofillClient::LocalSaveCardPromptCallback
        local_save_card_prompt_callback) {
  controller_.Show(web_contents(), AutofillClient::SaveCreditCardOptions(),
                   CreditCard(), std::move(upload_save_card_prompt_callback),
                   std::move(local_save_card_prompt_callback));
}

void SaveCardMessageControllerAndroidTest::TriggerPrimaryButtonClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

void SaveCardMessageControllerAndroidTest::DismissMessage() {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([](messages::MessageWrapper* message,
                   content::WebContents* web_contents,
                   messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(dismiss_reason));
      });
  controller_.DismissInternal();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// --- local save test ---
TEST_F(SaveCardMessageControllerAndroidTest, DismissOnPrimaryButtonClickLocal) {
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver;

  EnqueueMessage({}, mock_local_callback_receiver.Get());
  EXPECT_CALL(mock_local_callback_receiver, Run(AutofillClient::ACCEPTED));
  TriggerPrimaryButtonClick();
  DismissMessage();
}

TEST_F(SaveCardMessageControllerAndroidTest, DismissOnDeclineLocal) {
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver;

  EnqueueMessage({}, mock_local_callback_receiver.Get());
  EXPECT_CALL(mock_local_callback_receiver, Run(AutofillClient::DECLINED));
  DismissMessage();
}

// --- server save test ---
TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnPrimaryButtonClickUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;

  EnqueueMessage(mock_upload_callback_receiver.Get(), {});
  EXPECT_CALL(mock_upload_callback_receiver,
              Run(AutofillClient::ACCEPTED, testing::_));
  TriggerPrimaryButtonClick();
  DismissMessage();
}

TEST_F(SaveCardMessageControllerAndroidTest, DismissOnDeclineUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;

  EnqueueMessage(mock_upload_callback_receiver.Get(), {});
  EXPECT_CALL(mock_upload_callback_receiver,
              Run(AutofillClient::DECLINED, testing::_));
  DismissMessage();
}

}  // namespace autofill
