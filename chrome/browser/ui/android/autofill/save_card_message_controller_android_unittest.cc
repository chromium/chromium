// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_card_message_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/test/metrics/histogram_tester.h"
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
                          local_save_card_prompt_callback,
                      AutofillClient::SaveCreditCardOptions options);

  void DismissMessage();

  void TriggerPrimaryButtonClick();

  messages::MessageWrapper* GetMessageWrapper() {
    return controller_.message_.get();
  }

  void OnNameConfirmed() {
    JNIEnv* env = base::android::AttachCurrentThread();
    controller_.OnNameConfirmed(
        env, nullptr,
        base::android::JavaParamRef<jstring>(
            env,
            base::android::ConvertUTF8ToJavaString(env, "test").Release()));
  }

  void OnDateConfirmed() {
    JNIEnv* env = base::android::AttachCurrentThread();
    controller_.OnDateConfirmed(
        env, nullptr,
        base::android::JavaParamRef<jstring>(
            env, base::android::ConvertUTF8ToJavaString(env, "12").Release()),
        base::android::JavaParamRef<jstring>(
            env, base::android::ConvertUTF8ToJavaString(env, "25").Release()));
  }

  void OnPromoDismissed() {
    JNIEnv* env = base::android::AttachCurrentThread();
    controller_.PromptDismissed(env, nullptr);
  }

  bool IsDateConfirmed() { return controller_.is_date_confirmed_for_testing_; }

  bool IsNameConfirmed() { return controller_.is_name_confirmed_for_testing_; }

 private:
  SaveCardMessageControllerAndroid controller_;
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
    AutofillClient::LocalSaveCardPromptCallback local_save_card_prompt_callback,
    AutofillClient::SaveCreditCardOptions options) {
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage)
      .WillOnce(testing::Return(true));
  EXPECT_EQ(nullptr, GetMessageWrapper());
  controller_.Show(web_contents(), options, CreditCard(), {}, u"",
                   std::move(upload_save_card_prompt_callback),
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
  controller_.DismissMessage();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// --- local save test ---
TEST_F(SaveCardMessageControllerAndroidTest, DismissOnPrimaryButtonClickLocal) {
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver;
  EnqueueMessage({}, mock_local_callback_receiver.Get(), {});
  EXPECT_CALL(mock_local_callback_receiver, Run(AutofillClient::ACCEPTED));
  TriggerPrimaryButtonClick();
  DismissMessage();
}

TEST_F(SaveCardMessageControllerAndroidTest, DismissOnDeclineLocal) {
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver;

  EnqueueMessage({}, mock_local_callback_receiver.Get(), {});
  EXPECT_CALL(mock_local_callback_receiver, Run(AutofillClient::DECLINED));
  DismissMessage();
}

// --- server save test ---
TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnPrimaryButtonClickConfirmDateUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, {});
  TriggerPrimaryButtonClick();
  EXPECT_TRUE(IsDateConfirmed());
  DismissMessage();
}

TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnPrimaryButtonClickRequestDateUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  AutofillClient::SaveCreditCardOptions options;
  options.should_request_expiration_date_from_user = true;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, options);
  TriggerPrimaryButtonClick();
  EXPECT_TRUE(IsDateConfirmed());
  DismissMessage();
}

TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnPrimaryButtonClickConfirmNameUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;

  AutofillClient::SaveCreditCardOptions options;
  options.should_request_name_from_user = true;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, options);
  TriggerPrimaryButtonClick();
  EXPECT_TRUE(IsNameConfirmed());
  DismissMessage();
}

TEST_F(SaveCardMessageControllerAndroidTest, DismissOnDeclineUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, {});
  EXPECT_CALL(mock_upload_callback_receiver,
              Run(AutofillClient::DECLINED, testing::_));
  DismissMessage();
}
}  // namespace autofill
