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

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill::payments {

using autofill_metrics::MandatoryReauthOfferOptInDecision;

class MandatoryReauthManagerTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    std::unique_ptr<device_reauth::MockDeviceAuthenticator>
        mock_device_authenticator =
            std::make_unique<device_reauth::MockDeviceAuthenticator>();

    ON_CALL(*mock_device_authenticator,
            CanAuthenticateWithBiometricOrScreenLock)
        .WillByDefault(testing::Return(true));

    autofill_client_->SetDeviceAuthenticator(
        std::move(mock_device_authenticator));
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
  }

 protected:
  void ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision opt_in_decision) {
    histogram_tester_.ExpectUniqueSample(
        "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
        "ReauthOfferOptInDecision2",
        opt_in_decision, 1);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<MandatoryReauthManager> mandatory_reauth_manager_;
  base::HistogramTester histogram_tester_;
  CreditCard local_card_ = test::GetCreditCard();
  CreditCard server_card_ = test::GetMaskedServerCard();
  CreditCard virtual_card_ = test::GetVirtualCard();
};

// Test that `MandatoryReauthManager::Authenticate()` triggers
// `DeviceAuthenticator::AuthenticateWithMessage()`.
TEST_F(MandatoryReauthManagerTest, Authenticate) {
  EXPECT_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
              AuthenticateWithMessage)
      .Times(1);

  mandatory_reauth_manager_->Authenticate(
      /*callback=*/base::DoNothing());

  // Test that `MandatoryReauthManager::OnAuthenticationCompleted()` resets the
  // device authenticator.
  EXPECT_TRUE(mandatory_reauth_manager_->GetDeviceAuthenticatorPtrForTesting());
  mandatory_reauth_manager_->OnAuthenticationCompleted(
      /*callback=*/base::DoNothing(), /*success=*/true);
  EXPECT_FALSE(
      mandatory_reauth_manager_->GetDeviceAuthenticatorPtrForTesting());
}

// Test that `MandatoryReauthManager::AuthenticateWithMessage()` triggers
// `DeviceAuthenticator::AuthenticateWithMessage()`.
TEST_F(MandatoryReauthManagerTest, AuthenticateWithMessage) {
  EXPECT_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
              AuthenticateWithMessage)
      .Times(1);

  mandatory_reauth_manager_->AuthenticateWithMessage(
      /*message=*/u"Test", /*callback=*/base::DoNothing());

  // Test that `MandatoryReauthManager::OnAuthenticationCompleted()` resets the
  // device authenticator.
  EXPECT_TRUE(mandatory_reauth_manager_->GetDeviceAuthenticatorPtrForTesting());
  mandatory_reauth_manager_->OnAuthenticationCompleted(
      /*callback=*/base::DoNothing(), /*success=*/true);
  EXPECT_FALSE(
      mandatory_reauth_manager_->GetDeviceAuthenticatorPtrForTesting());
}

TEST_F(MandatoryReauthManagerTest, GetAuthenticationMethod_Biometric) {
  ON_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
          CanAuthenticateWithBiometrics)
      .WillByDefault(testing::Return(true));

  EXPECT_EQ(mandatory_reauth_manager_->GetAuthenticationMethod(),
            MandatoryReauthAuthenticationMethod::kBiometric);
}

TEST_F(MandatoryReauthManagerTest, GetAuthenticationMethod_ScreenLock) {
  ON_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
          CanAuthenticateWithBiometrics)
      .WillByDefault(testing::Return(false));
  ON_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
          CanAuthenticateWithBiometricOrScreenLock)
      .WillByDefault(testing::Return(true));

  EXPECT_EQ(mandatory_reauth_manager_->GetAuthenticationMethod(),
            MandatoryReauthAuthenticationMethod::kScreenLock);
}

TEST_F(MandatoryReauthManagerTest, GetAuthenticationMethod_UnsupportedMethod) {
  ON_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
          CanAuthenticateWithBiometrics)
      .WillByDefault(testing::Return(false));
  ON_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
          CanAuthenticateWithBiometricOrScreenLock)
      .WillByDefault(testing::Return(false));

  EXPECT_EQ(mandatory_reauth_manager_->GetAuthenticationMethod(),
            MandatoryReauthAuthenticationMethod::kUnsupportedMethod);
}

// Test that the MandatoryReauthManager returns that we should offer re-auth
// opt-in if the conditions for offering it are all met for local cards.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_LocalCard) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      CreditCard::RecordType::kLocalCard));
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
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
      CreditCard::RecordType::kLocalCard));
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
      CreditCard::RecordType::kLocalCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kIncognitoMode);
}

// Test that the MandatoryReauthManager returns that we should offer re-auth
// opt-in if the conditions for offering it are all met for virtual cards.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_VirtualCard) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      CreditCard::RecordType::kVirtualCard));
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the user has already made a decision on opting in or out of
// re-auth.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_UserAlreadyMadeDecision) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  mandatory_reauth_manager_->OnUserCancelledOptInPrompt();

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      CreditCard::RecordType::kLocalCard));
  EXPECT_TRUE(autofill_client_->GetPrefs()->GetUserPrefValue(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if authentication is not available on the device.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_AuthenticationNotAvailable) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  ON_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
          CanAuthenticateWithBiometricOrScreenLock)
      .WillByDefault(testing::Return(false));

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      CreditCard::RecordType::kLocalCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kNoSupportedReauthMethod);
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the conditions for offering re-auth are met, but the most recent
// filled card went through interactive authentication (or no card was
// autofilled at all).
TEST_F(
    MandatoryReauthManagerTest,
    ShouldOfferOptin_FilledCardWentThroughInteractiveAuthenticationOrNoAutofill) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  // 'card_identifier_if_non_interactive_authentication_flow_completed' is not
  // present, implying interactive authentication happened.
  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(absl::nullopt));
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
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);

  autofill_client_->GetPersonalDataManager()->AddCreditCard(local_card_);

  // Test that if the last filled card is the matching local card, we offer
  // re-auth opt-in.
  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      CreditCard::RecordType::kLocalCard));
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
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
  ON_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
          AuthenticateWithMessage)
      .WillByDefault(testing::WithArg<1>(
          [](base::OnceCallback<void(bool)> callback) {
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

  auto mock_device_authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  autofill_client_->SetDeviceAuthenticator(
      std::move(mock_device_authenticator2));

  ON_CALL(*autofill_client_->GetDeviceAuthenticatorPtr(),
          AuthenticateWithMessage)
      .WillByDefault(testing::WithArg<1>(
          [](base::OnceCallback<void(bool)> callback) {
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
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

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
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  EXPECT_EQ(autofill_client_->GetPrefs()->GetInteger(
                prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter),
            0);

  mandatory_reauth_manager_->OnUserClosedOptInPrompt();

  EXPECT_EQ(autofill_client_->GetPrefs()->GetInteger(
                prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter),
            1);
}

// Params of the MandatoryReauthManagerOptInFlowTest:
// -- CreditCard::RecordType record_type
class MandatoryReauthManagerOptInFlowTest
    : public MandatoryReauthManagerTest,
      public testing::WithParamInterface<CreditCard::RecordType> {
 public:
  MandatoryReauthManagerOptInFlowTest() = default;
  ~MandatoryReauthManagerOptInFlowTest() override = default;

  std::string GetOptInSource() {
    switch (GetParam()) {
      case CreditCard::RecordType::kLocalCard:
        return "CheckoutLocalCard";
      case CreditCard::RecordType::kFullServerCard:
        return "CheckoutFullServerCard";
      case CreditCard::RecordType::kVirtualCard:
        return "CheckoutVirtualCard";
      case CreditCard::RecordType::kMaskedServerCard:
        NOTREACHED();
        return "Unknown";
    }
  }

  void SetUpDeviceAuthenticator(
      device_reauth::MockDeviceAuthenticator* device_authenticator_ptr,
      bool success) {
    ON_CALL(*device_authenticator_ptr, AuthenticateWithMessage)
        .WillByDefault(testing::WithArg<1>(
            [success](base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(success);
            }));
  };
};

TEST_P(MandatoryReauthManagerOptInFlowTest, OptInSuccess) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;

  // Verify that we shall offer opt in.
  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(GetParam()));

  auto mock_device_authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  autofill_client_->SetDeviceAuthenticator(
      std::move(mock_device_authenticator2));

  SetUpDeviceAuthenticator(autofill_client_->GetDeviceAuthenticatorPtr(),
                           /*success=*/true);

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
          GetOptInSource() + ".OptIn",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent." +
          GetOptInSource() + ".OptIn",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded,
      1);
}

TEST_P(MandatoryReauthManagerOptInFlowTest, OptInShownButAuthFailure) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;

  // Verify that we shall offer opt in.
  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(GetParam()));

  auto mock_device_authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  autofill_client_->SetDeviceAuthenticator(
      std::move(mock_device_authenticator2));

  // Simulate authentication failure.
  SetUpDeviceAuthenticator(autofill_client_->GetDeviceAuthenticatorPtr(),
                           /*success=*/false);

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
          GetOptInSource() + ".OptIn",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent." +
          GetOptInSource() + ".OptIn",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowFailed, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MandatoryReauthManagerOptInFlowTest,
    testing::Values(CreditCard::RecordType::kLocalCard,
                    CreditCard::RecordType::kFullServerCard,
                    CreditCard::RecordType::kVirtualCard));

}  // namespace autofill::payments
