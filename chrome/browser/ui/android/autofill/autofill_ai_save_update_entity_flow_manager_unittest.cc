// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_ai_save_update_entity_flow_manager.h"

#include <optional>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_mode.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/autofill/autofill_message_model.h"
#include "chrome/browser/ui/autofill/autofill_message_model_test_api.h"
#include "chrome/browser/ui/autofill/mock_autofill_message_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using ::testing::_;
using ::testing::SaveArgByMove;

// TODO: crbug.com/460410690 - Cover different entity types.
class AutofillAiSaveUpdateEntityFlowManagerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            ChromeRenderViewHostTestHarness::profile());
    flow_manager_ = std::make_unique<AutofillAiSaveUpdateEntityFlowManager>(
        web_contents(), &autofill_message_controller_, "en-US");
  }

  void TearDown() override {
    identity_test_env_adaptor_.reset();
    flow_manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockAutofillMessageController& message_controller() {
    return autofill_message_controller_;
  }

  AutofillAiSaveUpdateEntityFlowManager& flow_manager() {
    return *flow_manager_;
  }

  base::MockCallback<AutofillClient::EntityImportPromptResultCallback>&
  prompt_closed_callback() {
    return prompt_closed_callback_;
  }

  EntityInstance new_entity(EntityInstance::RecordType record_type =
                                EntityInstance::RecordType::kLocal) {
    return test::GetPassportEntityInstance(
        {.name = u"Jon doe", .record_type = record_type});
  }

  EntityInstance old_entity(EntityInstance::RecordType record_type =
                                EntityInstance::RecordType::kLocal) {
    return test::GetPassportEntityInstance(
        {.name = u"Bob doe", .record_type = record_type});
  }

  void SigninUser(const std::string& email,
                  signin::ConsentLevel consent_level) {
    identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(email, consent_level);
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  MockAutofillMessageController autofill_message_controller_;
  base::MockCallback<AutofillClient::EntityImportPromptResultCallback>
      prompt_closed_callback_;
  std::unique_ptr<AutofillAiSaveUpdateEntityFlowManager> flow_manager_;
};

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest, ShowSaveMessage) {
  std::unique_ptr<AutofillMessageModel> message_model;
  // Show the message and save the message model.
  EXPECT_CALL(message_controller(), Show(_))
      .WillOnce(SaveArgByMove<0>(&message_model));
  flow_manager().OfferSave(new_entity(), /*old_entity=*/std::nullopt,
                           prompt_closed_callback().Get());

  EXPECT_EQ(test_api(*message_model).GetMessage().GetTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID));
  EXPECT_EQ(
      test_api(*message_model).GetMessage().GetDescription(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_SAVE_ENTITY_MESSAGE_SUBTITLE));
  EXPECT_EQ(test_api(*message_model).GetMessage().GetPrimaryButtonText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON));
}

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest, ShowUpdateMessage) {
  std::unique_ptr<AutofillMessageModel> message_model;
  // Show the message and save the message model.
  EXPECT_CALL(message_controller(), Show(_))
      .WillOnce(SaveArgByMove<0>(&message_model));
  flow_manager().OfferSave(new_entity(), old_entity(),
                           prompt_closed_callback().Get());

  EXPECT_EQ(test_api(*message_model).GetMessage().GetTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID));
  EXPECT_EQ(
      test_api(*message_model).GetMessage().GetDescription(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_SAVE_ENTITY_MESSAGE_SUBTITLE));
  EXPECT_EQ(
      test_api(*message_model).GetMessage().GetPrimaryButtonText(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_UPDATE_DIALOG_UPDATE_BUTTON));
}

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest, ShowSaveToWalletMessage) {
  SigninUser(TestingProfile::kDefaultProfileUserName,
             signin::ConsentLevel::kSignin);
  std::unique_ptr<AutofillMessageModel> message_model;
  // Show the message and save the message model.
  EXPECT_CALL(message_controller(), Show(_))
      .WillOnce(SaveArgByMove<0>(&message_model));
  flow_manager().OfferSave(
      new_entity(EntityInstance::RecordType::kServerWallet),
      /*old_entity=*/std::nullopt, prompt_closed_callback().Get());

  const std::u16string google_wallet =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);
  EXPECT_EQ(test_api(*message_model).GetMessage().GetTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID));
  EXPECT_EQ(
      test_api(*message_model).GetMessage().GetDescription(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_MESSAGE_SUBTITLE, google_wallet,
          base::UTF8ToUTF16(TestingProfile::kDefaultProfileUserName)));
  EXPECT_EQ(test_api(*message_model).GetMessage().GetPrimaryButtonText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_DIALOG_SAVE_BUTTON));
}

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest, ShowUpdateInWalletMessage) {
  SigninUser(TestingProfile::kDefaultProfileUserName,
             signin::ConsentLevel::kSignin);
  std::unique_ptr<AutofillMessageModel> message_model;
  // Show the message and save the message model.
  EXPECT_CALL(message_controller(), Show(_))
      .WillOnce(SaveArgByMove<0>(&message_model));
  flow_manager().OfferSave(
      new_entity(EntityInstance::RecordType::kServerWallet),
      old_entity(EntityInstance::RecordType::kServerWallet),
      prompt_closed_callback().Get());

  const std::u16string google_wallet =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);
  EXPECT_EQ(test_api(*message_model).GetMessage().GetTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE_ANDROID));
  EXPECT_EQ(
      test_api(*message_model).GetMessage().GetDescription(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_MESSAGE_SUBTITLE, google_wallet,
          base::UTF8ToUTF16(TestingProfile::kDefaultProfileUserName)));
  EXPECT_EQ(
      test_api(*message_model).GetMessage().GetPrimaryButtonText(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_UPDATE_DIALOG_UPDATE_BUTTON));
}

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest, ShowsMessage_MessageIngored) {
  std::unique_ptr<AutofillMessageModel> message_model;
  // Show the message and save the message model.
  EXPECT_CALL(message_controller(), Show(_))
      .WillOnce(SaveArgByMove<0>(&message_model));
  flow_manager().OfferSave(new_entity(), /*old_entity=*/std::nullopt,
                           prompt_closed_callback().Get());

  // Simulate the user ignoring the message which dismisses it.
  EXPECT_CALL(prompt_closed_callback(),
              Run(AutofillClient::AutofillAiBubbleResult::kNotInteracted));
  message_model->OnDismissed(messages::DismissReason::TIMER);
}

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest, ShowsMessage_MessageClosed) {
  std::unique_ptr<AutofillMessageModel> message_model;
  // Show the message and save the message model.
  EXPECT_CALL(message_controller(), Show(_))
      .WillOnce(SaveArgByMove<0>(&message_model));
  flow_manager().OfferSave(new_entity(), /*old_entity=*/std::nullopt,
                           prompt_closed_callback().Get());

  // Simulate the swipe on the message that closes it.
  EXPECT_CALL(prompt_closed_callback(),
              Run(AutofillClient::AutofillAiBubbleResult::kClosed));
  message_model->OnDismissed(messages::DismissReason::GESTURE);
}

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest,
       StartSecondFlowBeforePreviousIsFinished) {
  std::unique_ptr<AutofillMessageModel> message_model;
  // The message should be shown only once because the second flow is started
  // before the previous one is aborted.
  EXPECT_CALL(message_controller(), Show(_));
  flow_manager().OfferSave(new_entity(), /*old_entity=*/std::nullopt,
                           prompt_closed_callback().Get());
  flow_manager().OfferSave(new_entity(), /*old_entity=*/std::nullopt,
                           prompt_closed_callback().Get());
}

TEST_F(AutofillAiSaveUpdateEntityFlowManagerTest,
       StartSecondFlowAfterPreviousIsFinished) {
  std::unique_ptr<AutofillMessageModel> message_model;
  // The message should be shown twice because the second flow is started after
  // the previous one is aborted by the user.
  EXPECT_CALL(message_controller(), Show(_))
      .Times(2)
      .WillRepeatedly(SaveArgByMove<0>(&message_model));
  EXPECT_CALL(prompt_closed_callback(),
              Run(AutofillClient::AutofillAiBubbleResult::kClosed))
      .Times(2);

  flow_manager().OfferSave(new_entity(), /*old_entity=*/std::nullopt,
                           prompt_closed_callback().Get());
  // Dismiss the message first time.
  message_model->OnDismissed(messages::DismissReason::GESTURE);

  flow_manager().OfferSave(new_entity(), /*old_entity=*/std::nullopt,
                           prompt_closed_callback().Get());
  // Dismiss the message second time.
  message_model->OnDismissed(messages::DismissReason::GESTURE);
}

}  // namespace autofill
