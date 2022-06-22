// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_card_message_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/android/autofill/save_card_controller_metrics_android.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/save_credit_card_prompt_metrics.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {
constexpr char16_t kDefaultUrl[] = u"http://example.com";
static const char kServerPrefix[] = "Autofill.CreditCardMessage.Server";
static const char kLocalPrefix[] = "Autofill.CreditCardMessage.Local";
static const char kDialogPrefix[] = "Autofill.CreditCardMessage.DialogPrompt";
static const char kLocalResultPrefix[] =
    "Autofill.CreditCardSaveFlowResult.Local";
static const char kServerResultPrefix[] =
    "Autofill.CreditCardSaveFlowResult.Server";
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
  // Enqueue a message when another message is already being shown.
  void EnqueueAnotherMessage(AutofillClient::UploadSaveCardPromptCallback
                                 upload_save_card_prompt_callback,
                             AutofillClient::LocalSaveCardPromptCallback
                                 local_save_card_prompt_callback);

  void DismissMessage();
  void DismissMessage(messages::DismissReason reason);
  void ExpectDismiss() {
    EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
        .WillOnce([](messages::MessageWrapper* message,
                     messages::DismissReason dismiss_reason) {
          message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                         static_cast<int>(dismiss_reason));
        });
  }

  void TriggerPrimaryButtonClick();

  messages::MessageWrapper* GetMessageWrapper() {
    return controller_.message_.get();
  }

  void OnNameConfirmed() {
    JNIEnv* env = base::android::AttachCurrentThread();
    controller_.OnNameConfirmed(
        env,
        base::android::JavaParamRef<jstring>(
            env,
            base::android::ConvertUTF8ToJavaString(env, "test").Release()));
    OnConfirmationDialogDismissed();
  }

  void OnDateConfirmed() {
    JNIEnv* env = base::android::AttachCurrentThread();
    controller_.OnDateConfirmed(
        env,
        base::android::JavaParamRef<jstring>(
            env, base::android::ConvertUTF8ToJavaString(env, "12").Release()),
        base::android::JavaParamRef<jstring>(
            env, base::android::ConvertUTF8ToJavaString(env, "25").Release()));
    OnConfirmationDialogDismissed();
  }

  void OnConfirmationDialogDismissed() {
    JNIEnv* env = base::android::AttachCurrentThread();
    controller_.DialogDismissed(env);
  }

  void OnLinkClicked() {
    JNIEnv* env = base::android::AttachCurrentThread();
    controller_.OnLinkClicked(
        env, base::android::JavaParamRef<jstring>(
                 env, base::android::ConvertUTF16ToJavaString(env, u"").obj()));
  }

  void OnWebContentsFocused() { controller_.OnWebContentsFocused(); }

  bool IsDateConfirmed() { return controller_.is_date_confirmed_for_testing_; }

  bool IsNameConfirmed() { return controller_.is_name_confirmed_for_testing_; }

  bool IsSaveCardConfirmed() {
    return controller_.is_save_card_confirmed_for_testing_;
  }

  bool IsRestoreRequired() { return controller_.reprompt_required_; }

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
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  EXPECT_EQ(nullptr, GetMessageWrapper());
  controller_.Show(web_contents(), options, CreditCard(), {}, u"", u"",
                   std::move(upload_save_card_prompt_callback),
                   std::move(local_save_card_prompt_callback));
}

void SaveCardMessageControllerAndroidTest::EnqueueAnotherMessage(
    AutofillClient::UploadSaveCardPromptCallback
        upload_save_card_prompt_callback,
    AutofillClient::LocalSaveCardPromptCallback
        local_save_card_prompt_callback) {
  EXPECT_NE(nullptr, GetMessageWrapper());
  EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
  controller_.Show(web_contents(), AutofillClient::SaveCreditCardOptions(),
                   CreditCard(), {}, u"", u"",
                   std::move(upload_save_card_prompt_callback),
                   std::move(local_save_card_prompt_callback));
}

void SaveCardMessageControllerAndroidTest::TriggerPrimaryButtonClick() {
  GetMessageWrapper()->HandleActionClick(base::android::AttachCurrentThread());
}

void SaveCardMessageControllerAndroidTest::DismissMessage() {
  DismissMessage(messages::DismissReason::GESTURE);
}

void SaveCardMessageControllerAndroidTest::DismissMessage(
    messages::DismissReason reason) {
  EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
      .WillOnce([&reason](messages::MessageWrapper* message,
                          messages::DismissReason dismiss_reason) {
        message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                       static_cast<int>(reason));
      });
  controller_.DismissMessage();
  EXPECT_EQ(nullptr, GetMessageWrapper());
}

// --- local save test ---
TEST_F(SaveCardMessageControllerAndroidTest, DismissOnPrimaryButtonClickLocal) {
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver;
  base::HistogramTester histogram_tester;
  EnqueueMessage({}, mock_local_callback_receiver.Get(), {});
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT),
            GetMessageWrapper()->GetPrimaryButtonText());
  histogram_tester.ExpectBucketCount(kLocalPrefix, MessageMetrics::kShown, 1);
  EXPECT_CALL(mock_local_callback_receiver,
              Run(AutofillClient::SaveCardOfferUserDecision::kAccepted));
  TriggerPrimaryButtonClick();
  DismissMessage();
  histogram_tester.ExpectBucketCount(kLocalPrefix, MessageMetrics::kAccepted,
                                     1);
  histogram_tester.ExpectBucketCount(kLocalResultPrefix,
                                     SaveCreditCardPromptResult::kAccepted, 1);
}

TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnDeclineLocalFromDynamicChangeForm) {
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver;

  base::HistogramTester histogram_tester;
  AutofillClient::SaveCreditCardOptions options;
  options.from_dynamic_change_form = true;
  EnqueueMessage({}, mock_local_callback_receiver.Get(), options);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT),
            GetMessageWrapper()->GetPrimaryButtonText());
  histogram_tester.ExpectBucketCount(kLocalPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kLocalPrefix, ".FromDynamicChangeForm"}),
      MessageMetrics::kShown, 1);
  EXPECT_CALL(mock_local_callback_receiver,
              Run(AutofillClient::SaveCardOfferUserDecision::kDeclined));
  DismissMessage();
  histogram_tester.ExpectBucketCount(kLocalPrefix, MessageMetrics::kDenied, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kLocalPrefix, ".FromDynamicChangeForm"}),
      MessageMetrics::kDenied, 1);
  histogram_tester.ExpectBucketCount(kLocalResultPrefix,
                                     SaveCreditCardPromptResult::kDenied, 1);
}

TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnDeclineLocalFromNonFocusableForm) {
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver;

  base::HistogramTester histogram_tester;
  AutofillClient::SaveCreditCardOptions options;
  options.has_non_focusable_field = true;
  EnqueueMessage({}, mock_local_callback_receiver.Get(), options);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_INFOBAR_ACCEPT),
            GetMessageWrapper()->GetPrimaryButtonText());
  histogram_tester.ExpectBucketCount(kLocalPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kLocalPrefix, ".FromNonFocusableForm"}),
      MessageMetrics::kShown, 1);
  EXPECT_CALL(mock_local_callback_receiver,
              Run(AutofillClient::SaveCardOfferUserDecision::kDeclined));
  DismissMessage();
  histogram_tester.ExpectBucketCount(kLocalPrefix, MessageMetrics::kDenied, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kLocalPrefix, ".FromNonFocusableForm"}),
      MessageMetrics::kDenied, 1);
  histogram_tester.ExpectBucketCount(kLocalResultPrefix,
                                     SaveCreditCardPromptResult::kDenied, 1);
}

TEST_F(SaveCardMessageControllerAndroidTest,
       MessageEnqueueWhenAnotherMessageIsBeingDisplayedLocal) {
  // Enqueued when a message is already being shown.
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver;
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver2;

  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_local_callback_receiver,
              Run(AutofillClient::SaveCardOfferUserDecision::kIgnored));
  // Simulate the situation by enqueuing twice.
  EnqueueMessage({}, mock_local_callback_receiver.Get(), {});
  ExpectDismiss();
  EnqueueAnotherMessage({}, mock_local_callback_receiver2.Get());
  histogram_tester.ExpectBucketCount(kLocalPrefix, MessageMetrics::kIgnored, 1);
  histogram_tester.ExpectBucketCount(kLocalResultPrefix,
                                     SaveCreditCardPromptResult::kIgnored, 1);
  DismissMessage();
}

TEST_F(SaveCardMessageControllerAndroidTest, IgnoreMessageLocal) {
  // Enqueued when a message is already being shown.
  base::MockOnceCallback<void(
      AutofillClient::SaveCardOfferUserDecision user_decision)>
      mock_local_callback_receiver;

  base::HistogramTester histogram_tester;
  // Simulate the situation by enqueuing twice.
  EnqueueMessage({}, mock_local_callback_receiver.Get(), {});
  DismissMessage(messages::DismissReason::TIMER);
  histogram_tester.ExpectBucketCount(kLocalPrefix, MessageMetrics::kIgnored, 1);
  histogram_tester.ExpectBucketCount(kLocalResultPrefix,
                                     SaveCreditCardPromptResult::kIgnored, 1);
}

// --- server save test ---
// 1. Primary Button Accept
TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnPrimaryButtonClickRequestDateUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  AutofillClient::SaveCreditCardOptions options;
  options.should_request_expiration_date_from_user = true;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, options);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE),
            GetMessageWrapper()->GetPrimaryButtonText());
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

TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnPrimaryButtonClickConfirmSaveCardUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, {});
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_CONTINUE),
            GetMessageWrapper()->GetPrimaryButtonText());
  TriggerPrimaryButtonClick();
  EXPECT_TRUE(IsSaveCardConfirmed());
  DismissMessage();
}

// 2. Decline Message UI
TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnDeclineUploadFromDynamicChangeForm) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;

  base::HistogramTester histogram_tester;
  AutofillClient::SaveCreditCardOptions options;
  options.from_dynamic_change_form = true;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, options);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".FromDynamicChangeForm"}),
      MessageMetrics::kShown, 1);
  EXPECT_CALL(
      mock_upload_callback_receiver,
      Run(AutofillClient::SaveCardOfferUserDecision::kDeclined, testing::_));
  DismissMessage();
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kDenied, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".FromDynamicChangeForm"}),
      MessageMetrics::kDenied, 1);
  histogram_tester.ExpectBucketCount(kServerResultPrefix,
                                     SaveCreditCardPromptResult::kDenied, 1);
}

TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnDeclineUploadFromNonFocusableForm) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;

  base::HistogramTester histogram_tester;
  AutofillClient::SaveCreditCardOptions options;
  options.has_non_focusable_field = true;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, options);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".FromNonFocusableForm"}),
      MessageMetrics::kShown, 1);
  EXPECT_CALL(
      mock_upload_callback_receiver,
      Run(AutofillClient::SaveCardOfferUserDecision::kDeclined, testing::_));
  DismissMessage();
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kDenied, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".FromNonFocusableForm"}),
      MessageMetrics::kDenied, 1);
  histogram_tester.ExpectBucketCount(kServerResultPrefix,
                                     SaveCreditCardPromptResult::kDenied, 1);
}

// 3. Accept Dialog UI
TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnConfirmCardholderNameAcceptedUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  base::HistogramTester histogram_tester;
  AutofillClient::SaveCreditCardOptions options;
  options.should_request_name_from_user = true;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, options);
  EXPECT_CALL(
      mock_upload_callback_receiver,
      Run(AutofillClient::SaveCardOfferUserDecision::kAccepted, testing::_));
  // Triggering dialog will dismiss the message.
  TriggerPrimaryButtonClick();
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
  OnNameConfirmed();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".RequestingCardholderName"}),
      MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".RequestingCardholderName"}),
      MessageMetrics::kAccepted, 1);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kAccepted,
                                     1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".RequestingCardholderName"}),
      MessageDialogPromptMetrics::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat(
          {kDialogPrefix, ".RequestingCardholderName", ".DidClickLinks"}),
      MessageDialogPromptMetrics::kAccepted, 0);
  histogram_tester.ExpectBucketCount(kServerResultPrefix,
                                     SaveCreditCardPromptResult::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerResultPrefix, ".RequestingCardholderName"}),
      SaveCreditCardPromptResult::kAccepted, 1);
}

TEST_F(SaveCardMessageControllerAndroidTest, DismissOnConfirmDateAcceptUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  base::HistogramTester histogram_tester;
  AutofillClient::SaveCreditCardOptions options;
  options.should_request_expiration_date_from_user = true;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, options);
  EXPECT_CALL(
      mock_upload_callback_receiver,
      Run(AutofillClient::SaveCardOfferUserDecision::kAccepted, testing::_));
  TriggerPrimaryButtonClick();
  // Triggering dialog will dismiss the message.
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
  OnDateConfirmed();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".RequestingExpirationDate"}),
      MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".RequestingExpirationDate"}),
      MessageMetrics::kAccepted, 1);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kAccepted,
                                     1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".RequestingExpirationDate"}),
      MessageDialogPromptMetrics::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat(
          {kDialogPrefix, ".RequestingExpirationDate", ".DidClickLinks"}),
      MessageDialogPromptMetrics::kAccepted, 0);
  histogram_tester.ExpectBucketCount(kServerResultPrefix,
                                     SaveCreditCardPromptResult::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerResultPrefix, ".RequestingExpirationDate"}),
      SaveCreditCardPromptResult::kAccepted, 1);
}

TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnConfirmLegalLinesAcceptUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  base::HistogramTester histogram_tester;
  AutofillClient::SaveCreditCardOptions options;
  options.has_multiple_legal_lines = true;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, options);
  EXPECT_CALL(
      mock_upload_callback_receiver,
      Run(AutofillClient::SaveCardOfferUserDecision::kAccepted, testing::_));
  // Triggering dialog will dismiss the message.
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
  OnDateConfirmed();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".WithMultipleLegalLines"}),
      MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".WithMultipleLegalLines"}),
      MessageMetrics::kAccepted, 1);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kAccepted,
                                     1);
  histogram_tester.ExpectBucketCount(kServerResultPrefix,
                                     SaveCreditCardPromptResult::kAccepted, 1);
}

// 4. Decline Dialog UI
TEST_F(SaveCardMessageControllerAndroidTest, DismissOnPromoDismissedUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  base::HistogramTester histogram_tester;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, {});
  EXPECT_CALL(
      mock_upload_callback_receiver,
      Run(AutofillClient::SaveCardOfferUserDecision::kIgnored, testing::_));
  TriggerPrimaryButtonClick();
  // Triggering dialog will dismiss the message.
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
  OnConfirmationDialogDismissed();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kIgnored,
                                     1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".ConfirmInfo"}),
      MessageDialogPromptMetrics::kIgnored, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".ConfirmInfo", ".DidClickLinks"}),
      MessageDialogPromptMetrics::kIgnored, 0);
  histogram_tester.ExpectBucketCount(
      kServerResultPrefix, SaveCreditCardPromptResult::kInteractedAndIgnored,
      1);
}

TEST_F(SaveCardMessageControllerAndroidTest,
       DismissOnPromoDismissedUploadRecordedPreperly) {
  // Decline dialog first.
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  base::HistogramTester histogram_tester;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, {});
  EXPECT_CALL(
      mock_upload_callback_receiver,
      Run(AutofillClient::SaveCardOfferUserDecision::kIgnored, testing::_));
  TriggerPrimaryButtonClick();
  // Triggering dialog will dismiss the message.
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
  OnConfirmationDialogDismissed();
  EXPECT_EQ(nullptr, GetMessageWrapper());
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kIgnored,
                                     1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".ConfirmInfo"}),
      MessageDialogPromptMetrics::kIgnored, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".ConfirmInfo", ".DidClickLinks"}),
      MessageDialogPromptMetrics::kIgnored, 0);
  histogram_tester.ExpectBucketCount(
      kServerResultPrefix, SaveCreditCardPromptResult::kInteractedAndIgnored,
      1);

  // Trigger another message and dismiss it to test that no more dialog
  // related metric is record.
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver2;
  EnqueueMessage(mock_upload_callback_receiver2.Get(), {}, {});
  EXPECT_CALL(
      mock_upload_callback_receiver2,
      Run(AutofillClient::SaveCardOfferUserDecision::kIgnored, testing::_));
  DismissMessage(messages::DismissReason::TIMER);
  EXPECT_EQ(nullptr, GetMessageWrapper());
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kShown, 2);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kIgnored,
                                     2);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".ConfirmInfo"}),
      MessageDialogPromptMetrics::kIgnored, 1);  // expect no change.
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".ConfirmInfo", ".DidClickLinks"}),
      MessageDialogPromptMetrics::kIgnored, 0);  // expect no change.
  histogram_tester.ExpectBucketCount(
      kServerResultPrefix, SaveCreditCardPromptResult::kInteractedAndIgnored,
      1);  // expect no change.
  histogram_tester.ExpectBucketCount(kServerResultPrefix,
                                     SaveCreditCardPromptResult::kIgnored,
                                     1);  // new change.
}

// -- Others --
TEST_F(SaveCardMessageControllerAndroidTest, DialogRestoredOnTabSwitching) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  base::HistogramTester histogram_tester;
  AutofillClient::SaveCreditCardOptions options;
  options.should_request_expiration_date_from_user = true;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, options);
  EXPECT_CALL(
      mock_upload_callback_receiver,
      Run(AutofillClient::SaveCardOfferUserDecision::kAccepted, testing::_));

  // Triggering dialog will dismiss the message.
  DismissMessage(messages::DismissReason::PRIMARY_ACTION);
  OnLinkClicked();
  EXPECT_TRUE(IsRestoreRequired());
  OnWebContentsFocused();
  OnDateConfirmed();
  EXPECT_FALSE(IsRestoreRequired());
  EXPECT_EQ(nullptr, GetMessageWrapper());

  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".RequestingExpirationDate"}),
      MessageMetrics::kShown, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerPrefix, ".RequestingExpirationDate"}),
      MessageMetrics::kAccepted, 1);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kAccepted,
                                     1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".RequestingExpirationDate"}),
      MessageDialogPromptMetrics::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat(
          {kDialogPrefix, ".RequestingExpirationDate", ".DidClickLinks"}),
      MessageDialogPromptMetrics::kAccepted, 1);
  histogram_tester.ExpectBucketCount(kServerResultPrefix,
                                     SaveCreditCardPromptResult::kAccepted, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerResultPrefix, ".RequestingExpirationDate"}),
      SaveCreditCardPromptResult::kAccepted, 1);
}

TEST_F(SaveCardMessageControllerAndroidTest,
       MessageEnqueueWhenAnotherMessageIsBeingDisplayedUpload) {
  // Enqueued when a message is already being shown.
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver2;

  base::HistogramTester histogram_tester;
  // Simulate the situation by enqueuing twice.
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, {});

  EXPECT_CALL(
      mock_upload_callback_receiver,
      Run(AutofillClient::SaveCardOfferUserDecision::kIgnored, testing::_));
  ExpectDismiss();
  EnqueueAnotherMessage(mock_upload_callback_receiver2.Get(), {});
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kIgnored,
                                     1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".RequestingExpirationDate"}),
      MessageDialogPromptMetrics::kIgnored, 0);
  histogram_tester.ExpectBucketCount(kServerResultPrefix,
                                     SaveCreditCardPromptResult::kIgnored, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerResultPrefix, ".RequestingExpirationDate"}),
      SaveCreditCardPromptResult::kIgnored, 0);
  DismissMessage();
}

TEST_F(SaveCardMessageControllerAndroidTest, IgnoreMessageUpload) {
  base::MockOnceCallback<void(AutofillClient::SaveCardOfferUserDecision,
                              const AutofillClient::UserProvidedCardDetails&)>
      mock_upload_callback_receiver;

  base::HistogramTester histogram_tester;
  EnqueueMessage(mock_upload_callback_receiver.Get(), {}, {});
  DismissMessage(messages::DismissReason::TIMER);
  histogram_tester.ExpectBucketCount(kServerPrefix, MessageMetrics::kIgnored,
                                     1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kDialogPrefix, ".RequestingExpirationDate"}),
      MessageDialogPromptMetrics::kIgnored, 0);
  histogram_tester.ExpectBucketCount(kServerResultPrefix,
                                     SaveCreditCardPromptResult::kIgnored, 1);
  histogram_tester.ExpectBucketCount(
      base::StrCat({kServerResultPrefix, ".RequestingExpirationDate"}),
      SaveCreditCardPromptResult::kIgnored, 0);
}

}  // namespace autofill
