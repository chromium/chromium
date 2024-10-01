// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill::payments {

using autofill_metrics::MandatoryReauthOfferOptInDecision;
#if BUILDFLAG(IS_ANDROID)
using device_reauth::BiometricStatus;
#endif

class MandatoryReauthManagerTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    std::unique_ptr<device_reauth::MockDeviceAuthenticator>
        mock_device_authenticator =
            std::make_unique<device_reauth::MockDeviceAuthenticator>();

    autofill_client_->SetDeviceAuthenticator(
        std::move(mock_device_authenticator));
    mandatory_reauth_manager_ =
        std::make_unique<MandatoryReauthManager>(autofill_client_.get());
    SetUpAuthentication(/*biometrics_available=*/true,
                        /*screen_lock_available=*/true);
    autofill_client_->GetPersonalDataManager()->SetPrefService(
        autofill_client_->GetPrefs());
    test::SetCreditCardInfo(&server_card_, "Test User", "1111" /* Visa */,
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1");
  }

  device_reauth::MockDeviceAuthenticator& device_authenticator() {
    return *static_cast<device_reauth::MockDeviceAuthenticator*>(
        mandatory_reauth_manager_->GetDeviceAuthenticatorPtrForTesting());
  }

 protected:
  void ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision opt_in_decision) {
    histogram_tester_.ExpectUniqueSample(
        "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
        "ReauthOfferOptInDecision2",
        opt_in_decision, 1);
  }

  void SetUpAuthentication(bool biometrics_available,
                           bool screen_lock_available) {
#if BUILDFLAG(IS_ANDROID)
    BiometricStatus biometric_status = BiometricStatus::kUnavailable;
    if (biometrics_available) {
      biometric_status = BiometricStatus::kBiometricsAvailable;
    } else if (screen_lock_available) {
      biometric_status = BiometricStatus::kOnlyLskfAvailable;
    }
    ON_CALL(device_authenticator(), GetBiometricAvailabilityStatus)
        .WillByDefault(testing::Return(biometric_status));
#else
    ON_CALL(device_authenticator(), CanAuthenticateWithBiometrics)
        .WillByDefault(testing::Return(biometrics_available));
    ON_CALL(device_authenticator(), CanAuthenticateWithBiometricOrScreenLock)
        .WillByDefault(testing::Return(screen_lock_available));
#endif  // BUILDFLAG(IS_ANDROID)
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
  EXPECT_CALL(device_authenticator(), AuthenticateWithMessage).Times(1);

  mandatory_reauth_manager_->Authenticate(
      /*callback=*/base::DoNothing());
}

// Test that `MandatoryReauthManager::AuthenticateWithMessage()` triggers
// `DeviceAuthenticator::AuthenticateWithMessage()`.
TEST_F(MandatoryReauthManagerTest, AuthenticateWithMessage) {
  EXPECT_CALL(device_authenticator(), AuthenticateWithMessage).Times(1);

  mandatory_reauth_manager_->AuthenticateWithMessage(
      /*message=*/u"Test", /*callback=*/base::DoNothing());
}

TEST_F(MandatoryReauthManagerTest, GetAuthenticationMethod_Biometric) {
  SetUpAuthentication(/*biometrics_available=*/true,
                      /*screen_lock_available=*/true);

  EXPECT_EQ(mandatory_reauth_manager_->GetAuthenticationMethod(),
            MandatoryReauthAuthenticationMethod::kBiometric);
}

TEST_F(MandatoryReauthManagerTest, GetAuthenticationMethod_ScreenLock) {
  SetUpAuthentication(/*biometrics_available=*/false,
                      /*screen_lock_available=*/true);

  EXPECT_EQ(mandatory_reauth_manager_->GetAuthenticationMethod(),
            MandatoryReauthAuthenticationMethod::kScreenLock);
}

TEST_F(MandatoryReauthManagerTest, GetAuthenticationMethod_UnsupportedMethod) {
  SetUpAuthentication(/*biometrics_available=*/false,
                      /*screen_lock_available=*/false);

  EXPECT_EQ(mandatory_reauth_manager_->GetAuthenticationMethod(),
            MandatoryReauthAuthenticationMethod::kUnsupportedMethod);
}

// Test that the MandatoryReauthManager returns that we should offer re-auth
// opt-in if the conditions for offering it are all met for local cards.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_LocalCard) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  autofill_client_->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(local_card_);

  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      NonInteractivePaymentMethodType::kLocalCard));
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the conditions for offering it are all met but we are in off the
// record mode.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_Incognito) {
  autofill_client_->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(local_card_);

  autofill_client_->set_is_off_the_record(true);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      NonInteractivePaymentMethodType::kLocalCard));
  ExpectUniqueOfferOptInDecision(
      MandatoryReauthOfferOptInDecision::kIncognitoMode);
}

// Test that the MandatoryReauthManager returns that we should offer re-auth
// opt-in if the conditions for offering it are all met for virtual cards.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_VirtualCard) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      NonInteractivePaymentMethodType::kVirtualCard));
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
}

// Test that the MandatoryReauthManager returns that we should offer re-auth
// opt-in if the conditions for offering it are all met for masked server cards.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_MaskedServerCard) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      NonInteractivePaymentMethodType::kMaskedServerCard));
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if the user has already made a decision on opting in or out of
// re-auth.
TEST_F(MandatoryReauthManagerTest, ShouldOfferOptin_UserAlreadyMadeDecision) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  mandatory_reauth_manager_->OnUserCancelledOptInPrompt();

  autofill_client_->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      NonInteractivePaymentMethodType::kLocalCard));
  EXPECT_TRUE(autofill_client_->GetPrefs()->GetUserPrefValue(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
}

// Test that the MandatoryReauthManager returns that we should not offer re-auth
// opt-in if authentication is not available on the device.
TEST_F(MandatoryReauthManagerTest,
       ShouldOfferOptin_AuthenticationNotAvailable) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  SetUpAuthentication(/*biometrics_available=*/false,
                      /*screen_lock_available=*/false);

  autofill_client_->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(local_card_);

  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(
      NonInteractivePaymentMethodType::kLocalCard));
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
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  autofill_client_->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(local_card_);

  // 'card_identifier_if_non_interactive_authentication_flow_completed' is not
  // present, implying interactive authentication happened.
  EXPECT_FALSE(mandatory_reauth_manager_->ShouldOfferOptin(std::nullopt));
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
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  autofill_client_->GetPersonalDataManager()
      ->payments_data_manager()
      .AddCreditCard(local_card_);

  // Test that if the last filled card is the matching local card, we offer
  // re-auth opt-in.
  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(
      NonInteractivePaymentMethodType::kLocalCard));
  ExpectUniqueOfferOptInDecision(MandatoryReauthOfferOptInDecision::kOffered);
}

// Test that starting the re-auth opt-in flow will trigger the re-auth opt-in
// prompt to be shown.
TEST_F(MandatoryReauthManagerTest, StartOptInFlow) {
  mandatory_reauth_manager_->StartOptInFlow();
  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->GetMandatoryReauthOptInPromptWasShown());
}

// Test that the MandatoryReauthManager correctly handles the case where the
// user accepts the re-auth prompt.
TEST_F(MandatoryReauthManagerTest, OnUserAcceptedOptInPrompt) {
#if BUILDFLAG(IS_ANDROID)
  // Opt-in prompts are not shown on automotive as mandatory reauth is always
  // enabled.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  ON_CALL(device_authenticator(), AuthenticateWithMessage)
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
  EXPECT_FALSE(autofill_client_->GetPaymentsAutofillClient()
                   ->GetMandatoryReauthOptInPromptWasReshown());
  // Counter is increased by 1 since device authentication fails during opt in.
  EXPECT_EQ(autofill_client_->GetPrefs()->GetInteger(
                prefs::kAutofillPaymentMethodsMandatoryReauthPromoShownCounter),
            1);

  auto mock_device_authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  mandatory_reauth_manager_->SetDeviceAuthenticatorPtrForTesting(
      std::move(mock_device_authenticator2));

  ON_CALL(device_authenticator(), AuthenticateWithMessage)
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
  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->GetMandatoryReauthOptInPromptWasReshown());
  EXPECT_TRUE(autofill_client_->GetPrefs()->GetUserPrefValue(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
}

// Test that the MandatoryReauthManager correctly handles the case where the
// user cancels the re-auth prompt.
TEST_F(MandatoryReauthManagerTest, OnUserCancelledOptInPrompt) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
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
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
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
// -- NonInteractivePaymentMethodType non-interactive payment method type
class MandatoryReauthManagerOptInFlowTest
    : public MandatoryReauthManagerTest,
      public testing::WithParamInterface<NonInteractivePaymentMethodType> {
 protected:
  void SetUp() override {
    MandatoryReauthManagerTest::SetUp();
    mandatory_reauth_manager_->SetDeviceAuthenticatorPtrForTesting(
        std::make_unique<device_reauth::MockDeviceAuthenticator>());
    SetUpAuthentication(/*biometrics_available=*/true,
                        /*screen_lock_available=*/true);
  }

  std::string GetOptInSource() {
    switch (GetParam()) {
      case NonInteractivePaymentMethodType::kLocalCard:
        return "CheckoutLocalCard";
      case NonInteractivePaymentMethodType::kFullServerCard:
        return "CheckoutFullServerCard";
      case NonInteractivePaymentMethodType::kVirtualCard:
        return "CheckoutVirtualCard";
      case NonInteractivePaymentMethodType::kMaskedServerCard:
        return "CheckoutMaskedServerCard";
      case NonInteractivePaymentMethodType::kLocalIban:
        return "CheckoutLocalIban";
      case NonInteractivePaymentMethodType::kServerIban:
        return "CheckoutServerIban";
    }
  }

  std::string GetHistogramStringForNonInteractivePaymentMethodType() {
    switch (GetParam()) {
      case NonInteractivePaymentMethodType::kLocalCard:
        return "LocalCard";
      case NonInteractivePaymentMethodType::kFullServerCard:
      case NonInteractivePaymentMethodType::kMaskedServerCard:
        return "ServerCard";
      case NonInteractivePaymentMethodType::kVirtualCard:
        return "VirtualCard";
      case NonInteractivePaymentMethodType::kLocalIban:
        return "LocalIban";
      case NonInteractivePaymentMethodType::kServerIban:
        return "ServerIban";
    }
  }

  void SetUpDeviceAuthenticator(bool success) {
    ON_CALL(device_authenticator(), AuthenticateWithMessage)
        .WillByDefault(testing::WithArg<1>(
            [success](base::OnceCallback<void(bool)> callback) {
              std::move(callback).Run(success);
            }));
  }
};

TEST_P(MandatoryReauthManagerOptInFlowTest,
       StartDeviceAuthentication_Biometric) {
  base::HistogramTester histogram_tester;
  SetUpAuthentication(/*biometrics_available=*/true,
                      /*screen_lock_available=*/true);

  EXPECT_CALL(device_authenticator(), AuthenticateWithMessage);
  mandatory_reauth_manager_->StartDeviceAuthentication(GetParam(),
                                                       base::DoNothing());
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage." +
          GetHistogramStringForNonInteractivePaymentMethodType() + ".Biometric",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
      1);
}

TEST_P(MandatoryReauthManagerOptInFlowTest,
       StartDeviceAuthentication_ScreenLock) {
  base::HistogramTester histogram_tester;

  SetUpAuthentication(/*biometrics_available=*/false,
                      /*screen_lock_available=*/true);

  mandatory_reauth_manager_->StartDeviceAuthentication(GetParam(),
                                                       base::DoNothing());
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage." +
          GetHistogramStringForNonInteractivePaymentMethodType() +
          ".ScreenLock",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
      1);
}

TEST_P(MandatoryReauthManagerOptInFlowTest,
       StartDeviceAuthentication_Unsupported) {
  base::HistogramTester histogram_tester;
  SetUpAuthentication(/*biometrics_available=*/false,
                      /*screen_lock_available=*/false);

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(true));
  mandatory_reauth_manager_->StartDeviceAuthentication(GetParam(),
                                                       callback.Get());
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage." +
          GetHistogramStringForNonInteractivePaymentMethodType() +
          ".UnsupportedMethod",
      autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowSkipped,
      1);
}

TEST_P(MandatoryReauthManagerOptInFlowTest, OptInSuccess) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;

  // Verify that we shall offer opt in.
  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(GetParam()));

  auto mock_device_authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  mandatory_reauth_manager_->SetDeviceAuthenticatorPtrForTesting(
      std::move(mock_device_authenticator2));

  SetUpDeviceAuthenticator(/*success=*/true);

  // Start OptIn flow.
  static_cast<MandatoryReauthManager*>(mandatory_reauth_manager_.get())
      ->StartOptInFlow();
  // Simulate user accepts the opt in prompt.
  mandatory_reauth_manager_->OnUserAcceptedOptInPrompt();

  EXPECT_TRUE(autofill_client_->GetPrefs()->GetBoolean(
      prefs::kAutofillPaymentMethodsMandatoryReauth));
  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->GetMandatoryReauthOptInPromptWasShown());
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
    // Skip the test for automotive as Mandatory Re-auth should always be turned
    // on for automotive users.
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;

  // Verify that we shall offer opt in.
  EXPECT_TRUE(mandatory_reauth_manager_->ShouldOfferOptin(GetParam()));

  auto mock_device_authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  mandatory_reauth_manager_->SetDeviceAuthenticatorPtrForTesting(
      std::move(mock_device_authenticator2));

  // Simulate authentication failure.
  SetUpDeviceAuthenticator(/*success=*/false);

  // Start OptIn flow.
  static_cast<MandatoryReauthManager*>(mandatory_reauth_manager_.get())
      ->StartOptInFlow();
  // Simulate user accepts the opt in prompt. But the device authentication
  // fails.
  mandatory_reauth_manager_->OnUserAcceptedOptInPrompt();

  EXPECT_TRUE(autofill_client_->GetPaymentsAutofillClient()
                  ->GetMandatoryReauthOptInPromptWasShown());
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
    testing::ValuesIn(MandatoryReauthManager::
                          GetAllNonInteractivePaymentMethodTypesForTesting()));

}  // namespace autofill::payments
