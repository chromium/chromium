// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "base/strings/utf_string_conversions.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class MandatoryReauthManagerTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    mock_device_authenticator_ =
        static_cast<device_reauth::MockDeviceAuthenticator*>(
            autofill_client_->GetDeviceAuthenticator().get());
    mandatory_reauth_manager_ =
        std::make_unique<MandatoryReauthManager>(autofill_client_.get());
    autofill_client_->GetPersonalDataManager()->Init(
        /*profile_database=*/nullptr,
        /*account_database=*/nullptr,
        /*pref_service=*/autofill_client_->GetPrefs(),
        /*local_state=*/autofill_client_->GetPrefs(),
        /*identity_manager=*/nullptr,
        /*history_service=*/nullptr,
        /*sync_service=*/nullptr,
        /*strike_database=*/nullptr,
        /*image_fetcher=*/nullptr,
        /*is_off_the_record=*/false);
    test::SetCreditCardInfo(&server_card_, "Test User", "1111" /* Visa */,
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1");
    SetCanAuthenticateWithBiometrics(true);
  }

 protected:
  void SetCanAuthenticateWithBiometrics(bool value) {
    ON_CALL(*mock_device_authenticator_, CanAuthenticateWithBiometrics)
        .WillByDefault(testing::Return(value));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  raw_ptr<device_reauth::MockDeviceAuthenticator> mock_device_authenticator_;
  std::unique_ptr<MandatoryReauthManager> mandatory_reauth_manager_;
  CreditCard local_card_ = test::GetCreditCard();
  CreditCard server_card_ = test::GetMaskedServerCard();
  CreditCard virtual_card_ = test::GetVirtualCard();
};

// Test that the MandatoryReauthManager returns that we should offer re-auth
// opt-in if the conditions for offering it are all met for local cards.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_LocalCard) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kLocalCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the card identifier stored has the last four digits instead of a
// GUID. This can occur if a user encounters non-interactive authentication with
// a virtual card autofill, but then deletes the card in the form and manually
// types in a known local card. For test thoroughness of edge cases, we have
// made the last four digits be the same as the last four digits of the local
// card.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_LocalCard_InvalidCardIdentifier) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_,
      FormDataImporter::CardLastFourDigits(
          base::UTF16ToUTF8(local_card_.LastFourDigits())),
      FormDataImporter::kLocalCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the conditions for offering it are all met, but the feature flag is
// off.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_LocalCard_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kLocalCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the conditions for offering it are all met but we are in off the
// record mode.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_OffTheRecord) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  autofill_client_->set_is_off_the_record(true);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kLocalCard));
}

// Test that the MandatoryReauthManager returns that we should offer re-auth
// opt-in if the conditions for offering it are all met for virtual cards.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_VirtualCard) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      virtual_card_,
      FormDataImporter::CardLastFourDigits(
          base::UTF16ToUTF8(virtual_card_.LastFourDigits())),
      FormDataImporter::kVirtualCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in for a virtual card if the card identifier is a GUID instead of a last
// four digits.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_VirtualCard_InvalidCardIdentifier) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      virtual_card_, FormDataImporter::CardGuid(virtual_card_.guid()),
      FormDataImporter::kVirtualCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the conditions if the last four digits in the virtual card case do
// not match.
TEST_F(MandatoryReauthManagerTest,

       ShouldOfferOptin_LastFourDigitsDontMatch_VirtualCard) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      virtual_card_, FormDataImporter::CardLastFourDigits("1234"),
      FormDataImporter::kVirtualCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if we did not extract any card from the form.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_NoCardExtractedFromForm) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      absl::nullopt, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kLocalCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the user has already made a decision on opting in or out of
// re-auth.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_UserAlreadyMadeDecision) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  mandatory_reauth_manager_->OnUserCancelledOptInPrompt();

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kLocalCard));
  EXPECT_TRUE(autofill_client_->GetPrefs()->GetUserPrefValue(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if biometrics are not available on the device.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_BiometricsNotAvailable) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  SetCanAuthenticateWithBiometrics(false);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kLocalCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the conditions for offering re-auth are met, but any card autofills
// that occurred required interactive authentication.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_AllCardAutofillsRequiredInteractiveAuthentication) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, absl::nullopt, FormDataImporter::kLocalCard));
}

// Test that the MandatoryReauthManager returns that we should offer re-auth
// opt-in if we have a matching local card for a server card submitted in the
// form.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_ServerCardWithMatchingLocalCard) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      server_card_, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kServerCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if we do not have a matching local card for a server card submitted in
// the form.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_ServerCardWithNoMatchingLocalCard) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(server_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      server_card_, FormDataImporter::CardGuid(server_card_.guid()),
      FormDataImporter::kServerCard));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if we do not have a stored card that matches the card extracted from
// the form.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_NoStoredCardForExtractedCard) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      server_card_, FormDataImporter::CardGuid(server_card_.guid()),
      FormDataImporter::kServerCard));
}

// Test that starting the re-auth opt-in flow will trigger the re-auth opt-in
// prompt to be shown.
TEST_F(MandatoryReauthManagerTest, StartOptInFlow) {
  mandatory_reauth_manager_->StartOptInFlow();
  EXPECT_TRUE(autofill_client_->GetMandatoryReauthOptInPromptWasShown());
}

// Test that the MandatoryReauthManager correctly handles the case where the
// user accepts the re-auth prompt.
TEST_F(MandatoryReauthManagerTest, OnUserAcceptedOptInPrompt) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*mock_device_authenticator_, AuthenticateWithMessage)
#elif BUILDFLAG(IS_ANDROID)
  ON_CALL(*mock_device_authenticator_, Authenticate)
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      .WillByDefault(
          testing::WithArg<1>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          }));

  // We need to call `StartOptInFlow()` here to ensure the device
  // authenticator gets set.
  static_cast<MandatoryReauthManager*>(mandatory_reauth_manager_.get())
      ->StartOptInFlow();
  mandatory_reauth_manager_->OnUserAcceptedOptInPrompt();

  EXPECT_FALSE(autofill_client_->GetPrefs()->GetBoolean(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
  EXPECT_FALSE(autofill_client_->GetMandatoryReauthOptInPromptWasReshown());

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  ON_CALL(*mock_device_authenticator_, AuthenticateWithMessage)
#elif BUILDFLAG(IS_ANDROID)
  ON_CALL(*mock_device_authenticator_, Authenticate)
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      .WillByDefault(
          testing::WithArg<1>([](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  // We need to call `StartOptInFlow()` here to ensure the device
  // authenticator gets set.
  static_cast<MandatoryReauthManager*>(mandatory_reauth_manager_.get())
      ->StartOptInFlow();
  mandatory_reauth_manager_->OnUserAcceptedOptInPrompt();

  EXPECT_TRUE(autofill_client_->GetPrefs()->GetBoolean(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
  EXPECT_TRUE(autofill_client_->GetMandatoryReauthOptInPromptWasReshown());
  EXPECT_EQ(autofill_client_->GetPrefs()->GetInteger(
                prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter),
            1);
  EXPECT_TRUE(autofill_client_->GetPrefs()->GetUserPrefValue(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
}

// Test that the MandatoryReauthManager correctly handles the case where the
// user cancels the re-auth prompt.
TEST_F(MandatoryReauthManagerTest, OnUserCancelledOptInPrompt) {
  EXPECT_FALSE(autofill_client_->GetPrefs()->GetUserPrefValue(
      prefs::kAutofillPaymentMethodsMandatoryReauth));

  mandatory_reauth_manager_->OnUserCancelledOptInPrompt();

  EXPECT_TRUE(autofill_client_->GetPrefs()->GetUserPrefValue(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
  EXPECT_FALSE(autofill_client_->GetPrefs()->GetBoolean(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
}

// Test that the MandatoryReauthManager correctly handles the case where the
// user closed the re-auth prompt.
TEST_F(MandatoryReauthManagerTest, OnUserClosedOptInPrompt) {
  EXPECT_EQ(autofill_client_->GetPrefs()->GetInteger(
                prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter),
            0);

  mandatory_reauth_manager_->OnUserClosedOptInPrompt();

  EXPECT_EQ(autofill_client_->GetPrefs()->GetInteger(
                prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter),
            1);
}

}  // namespace autofill::payments
