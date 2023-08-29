// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "base/strings/utf_string_conversions.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
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

using autofill_metrics::MandatoryReauthOfferOptInDecision;

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
        /*image_fetcher=*/nullptr);
    test::SetCreditCardInfo(&server_card_, "Test User", "1111" /* Visa */,
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1");
    SetCanAuthenticate(true);
  }

 protected:
  void SetCanAuthenticate(bool value) {
    ON_CALL(*mock_device_authenticator_,
            CanAuthenticateWithBiometricOrScreenLock)
        .WillByDefault(testing::Return(value));
  }

  void ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision opt_in_decision) {
    histogram_tester_.ExpectUniqueSample(
        "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
        "ReauthOfferOptInDecision",
        opt_in_decision, 1);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  raw_ptr<device_reauth::MockDeviceAuthenticator> mock_device_authenticator_;
  std::unique_ptr<MandatoryReauthManager> mandatory_reauth_manager_;
  base::HistogramTester histogram_tester_;
  CreditCard local_card_ = test::GetCreditCard();
  CreditCard server_card_ = test::GetMaskedServerCard();
  CreditCard virtual_card_ = test::GetVirtualCard();
};

// Params of the MandatoryReauthManagerOptInFlowTest:
// -- autofill::FormDataImporter::CreditCardImportType CreditCardImportType
class MandatoryReauthManagerOptInFlowTest
    : public MandatoryReauthManagerTest,
      public testing::WithParamInterface<
          FormDataImporter::CreditCardImportType> {
 public:
  MandatoryReauthManagerOptInFlowTest() = default;
  ~MandatoryReauthManagerOptInFlowTest() override = default;

  CreditCard GetCreditCardBasedOnParam() {
    switch (GetParam()) {
      case FormDataImporter::kLocalCard:
        autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);
        return local_card_;
      case FormDataImporter::kServerCard:
        return server_card_;
      case FormDataImporter::kVirtualCard:
        return virtual_card_;
      default:
        NOTREACHED();
        return local_card_;
    }
  }

  absl::variant<FormDataImporter::CardGuid,
                FormDataImporter::CardLastFourDigits>
  GetCardIdentifierBasedOnParam() {
    switch (GetParam()) {
      case FormDataImporter::kLocalCard:
        return FormDataImporter::CardGuid(local_card_.guid());
      case FormDataImporter::kServerCard:
        // For Server card, the only opt in case is if it had a matching local
        // card.
        autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);
        return FormDataImporter::CardGuid(local_card_.guid());
      case FormDataImporter::kVirtualCard:
        return FormDataImporter::CardLastFourDigits(
            base::UTF16ToUTF8(virtual_card_.LastFourDigits()));
      default:
        NOTREACHED();
        return FormDataImporter::CardGuid(local_card_.guid());
    }
  }

  std::string GetOtpInSource() {
    switch (GetParam()) {
      case FormDataImporter::kLocalCard:
        return "CheckoutLocalCard";
      case FormDataImporter::kServerCard:
        return "CheckoutLocalCard";
      case FormDataImporter::kVirtualCard:
        return "CheckoutVirtualCard";
      default:
        NOTREACHED();
        return "Unknown";
    }
  }

  void SetUpDeviceAuthenticator(bool success) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    ON_CALL(*mock_device_authenticator_, AuthenticateWithMessage)
#elif BUILDFLAG(IS_ANDROID)
    ON_CALL(*mock_device_authenticator_, Authenticate)
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
        .WillByDefault(testing::WithArg<1>(
            [success](base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(success);
            }));
  }
};

// Test that `MandatoryReauthManager::Authenticate()` triggers
// `DeviceAuthenticator::Authenticate()`.
TEST_F(MandatoryReauthManagerTest, Authenticate) {
  EXPECT_CALL(*mock_device_authenticator_, Authenticate).Times(1);

  mandatory_reauth_manager_->Authenticate(
      /*requester=*/device_reauth::DeviceAuthRequester::kLocalCardAutofill,
      /*callback=*/base::DoNothing());

  // Test that `MandatoryReauthManager::OnAuthenticationCompleted()` resets the
  // device authenticator.
  EXPECT_TRUE(mandatory_reauth_manager_->GetDeviceAuthenticatorForTesting());
  mandatory_reauth_manager_->OnAuthenticationCompleted(
      /*callback=*/base::DoNothing(), /*success=*/true);
  EXPECT_FALSE(mandatory_reauth_manager_->GetDeviceAuthenticatorForTesting());
}

// Test that `MandatoryReauthManager::AuthenticateWithMessage()` triggers
// `DeviceAuthenticator::AuthenticateWithMessage()`.
TEST_F(MandatoryReauthManagerTest, AuthenticateWithMessage) {
  EXPECT_CALL(*mock_device_authenticator_, AuthenticateWithMessage).Times(1);

  mandatory_reauth_manager_->AuthenticateWithMessage(
      /*message=*/u"Test", /*callback=*/base::DoNothing());

  // Test that `MandatoryReauthManager::OnAuthenticationCompleted()` resets the
  // device authenticator.
  EXPECT_TRUE(mandatory_reauth_manager_->GetDeviceAuthenticatorForTesting());
  mandatory_reauth_manager_->OnAuthenticationCompleted(
      /*callback=*/base::DoNothing(), /*success=*/true);
  EXPECT_FALSE(mandatory_reauth_manager_->GetDeviceAuthenticatorForTesting());
}

TEST_F(MandatoryReauthManagerTest, GetAuthenticationMethod_Biometric) {
  ON_CALL(*mock_device_authenticator_, CanAuthenticateWithBiometrics)
      .WillByDefault(testing::Return(true));

  EXPECT_EQ(mandatory_reauth_manager_->GetAuthenticationMethod(),
            MandatoryReauthAuthenticationMethod::kBiometric);
}

TEST_F(MandatoryReauthManagerTest, GetAuthenticationMethod_ScreenLock) {
  ON_CALL(*mock_device_authenticator_, CanAuthenticateWithBiometrics)
      .WillByDefault(testing::Return(false));
  ON_CALL(*mock_device_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillByDefault(testing::Return(true));

  EXPECT_EQ(mandatory_reauth_manager_->GetAuthenticationMethod(),
            MandatoryReauthAuthenticationMethod::kScreenLock);
}

TEST_F(MandatoryReauthManagerTest, GetAuthenticationMethod_UnsupportedMethod) {
  ON_CALL(*mock_device_authenticator_, CanAuthenticateWithBiometrics)
      .WillByDefault(testing::Return(false));
  ON_CALL(*mock_device_authenticator_, CanAuthenticateWithBiometricOrScreenLock)
      .WillByDefault(testing::Return(false));

  EXPECT_EQ(mandatory_reauth_manager_->GetAuthenticationMethod(),
            MandatoryReauthAuthenticationMethod::kUnsupportedMethod);
}

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
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
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
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kManuallyFilledLocalCard);
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
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_Incognito) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  autofill_client_->set_is_off_the_record(true);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kLocalCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kIncognitoMode);
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
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in for a virtual card if the card identifier is a GUID instead of a last
// four digits.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_VirtualCard_InvalidCardIdentifier) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  // `card_identifier_if_non_interactive_authentication_flow_completed` holds no
  // card last four digits, which means that the card that was most recently
  // filled with non-interactive authentication was not a virtual card. This is
  // possible when a user goes through a non-interactive authentication flow
  // with a card that is not a virtual card, then types in a virtual card
  // manually into the form.
  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      virtual_card_, FormDataImporter::CardGuid(virtual_card_.guid()),
      FormDataImporter::kVirtualCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kManuallyFilledVirtualCard);
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
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kNoStoredCardForExtractedCard);
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
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kNoCardExtractedFromForm);
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
// opt-in if authentication is not available on the device.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_AuthenticationNotAvailable) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  SetCanAuthenticate(false);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kLocalCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kNoSupportedReauthMethod);
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the conditions for offering re-auth are met, but any filled card
// went through interactive authentication.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_FilledCardWentThroughInteractiveAuthentication) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  // 'card_identifier_if_non_interactive_authentication_flow_completed' is not
  // present, implying interactive authentication happened.
  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, absl::nullopt, FormDataImporter::kLocalCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kWentThroughInteractiveAuthentication);
}

// Test that the MandatoryReauthManager returns that we should offer re-auth
// opt-in if we have a matching local card for a server card extracted from the
// form, and the matching local card was the last filled card. This also tests
// that the metrics logged correctly.
TEST_F(
    MandatoryReauthManagerTest,
    ShouldOfferOptin_ServerCardWithMatchingLocalCard_LastFilledCardWasLocalCard) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  // Test that if the last filled card is the matching local card, we offer
  // re-auth opt-in.
  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      server_card_, FormDataImporter::CardGuid(local_card_.guid()),
      FormDataImporter::kServerCard));
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if we have a matching local card for a server card extracted from the
// form, and the matching local card was not the last filled card. This also
// tests that the metrics logged correctly.
TEST_F(
    MandatoryReauthManagerTest,
    ShouldOfferOptin_ServerCardWithMatchingLocalCard_LastFilledCardWasServerCard) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  // Test that if the last filled card is not the matching local card, we do not
  // offer re-auth opt-in.
  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      server_card_, FormDataImporter::CardGuid(server_card_.guid()),
      FormDataImporter::kServerCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kUnsupportedCardType);
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
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kUnsupportedCardType);
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the card identifier stored has the last four digits instead of a
// GUID. This can occur if a user encounters non-interactive authentication with
// a server card autofill, but then deletes the card in the form and manually
// types in a server card.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_ServerCard_InvalidCardIdentifier) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(server_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      server_card_,
      FormDataImporter::CardLastFourDigits(
          base::UTF16ToUTF8(server_card_.LastFourDigits())),
      FormDataImporter::kServerCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kManuallyFilledServerCard);
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if we do not have a stored card that matches the card extracted from
// the form.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_NoStoredCardForExtractedCard) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      local_card_, FormDataImporter::CardGuid(test::GetCreditCard2().guid()),
      FormDataImporter::kLocalCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kNoStoredCardForExtractedCard);
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
  // Counter is increased by 1 since device authentication fails during opt in.
  EXPECT_EQ(autofill_client_->GetPrefs()->GetInteger(
                prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter),
            1);

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

TEST_P(MandatoryReauthManagerOptInFlowTest, OptInSuccess) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;

  // Verify that we shall offer opt in.
  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      GetCreditCardBasedOnParam(), GetCardIdentifierBasedOnParam(),
      GetParam()));

  SetUpDeviceAuthenticator(/*success=*/true);

  // Start OptIn flow.
  static_cast<MandatoryReauthManager*>(mandatory_reauth_manager_.get())
      ->StartOptInFlow();
  // Simulate user accepts the opt in prompt.
  mandatory_reauth_manager_->OnUserAcceptedOptInPrompt();

  EXPECT_TRUE(autofill_client_->GetPrefs()->GetBoolean(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
  EXPECT_TRUE(autofill_client_->GetMandatoryReauthOptInPromptWasShown());
  // Counter is not changed since it's a successful opt in.
  EXPECT_EQ(autofill_client_->GetPrefs()->GetInteger(
                prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter),
            0);
  EXPECT_TRUE(autofill_client_->GetPrefs()->GetUserPrefValue(
      prefs::kAutofillPaymentMethodsMandatoryReauth));

  // Ensures the metrics have been logged correctly.
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent." +
          GetOtpInSource() + ".OptIn",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent." +
          GetOtpInSource() + ".OptIn",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded,
      1);
}

TEST_P(MandatoryReauthManagerOptInFlowTest, OptInShownButAuthFailure) {
  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;

  // Verify that we shall offer opt in.
  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      GetCreditCardBasedOnParam(), GetCardIdentifierBasedOnParam(),
      GetParam()));

  // Simulate authentication failure.
  SetUpDeviceAuthenticator(/*success=*/false);

  // Start OptIn flow.
  static_cast<MandatoryReauthManager*>(mandatory_reauth_manager_.get())
      ->StartOptInFlow();
  // Simulate user accepts the opt in prompt. But the device authentication
  // fails.
  mandatory_reauth_manager_->OnUserAcceptedOptInPrompt();

  EXPECT_TRUE(autofill_client_->GetMandatoryReauthOptInPromptWasShown());
  // Counter is increased by 1 since device authentication fails during opt in.
  EXPECT_EQ(autofill_client_->GetPrefs()->GetInteger(
                prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter),
            1);
  // The reauth pref is still off since authentication fails.
  EXPECT_FALSE(autofill_client_->GetPrefs()->GetBoolean(
      prefs::kAutofillPaymentMethodsMandatoryReauth));

  // Ensures the metrics have been logged correctly.
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent." +
          GetOtpInSource() + ".OptIn",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent." +
          GetOtpInSource() + ".OptIn",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowFailed, 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         MandatoryReauthManagerOptInFlowTest,
                         testing::Values(FormDataImporter::kLocalCard,
                                         FormDataImporter::kServerCard,
                                         FormDataImporter::kVirtualCard));

}  // namespace autofill::payments
