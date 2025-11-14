// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_base.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/test/mock_mandatory_reauth_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace autofill {
namespace {

using ::base::test::RunOnceCallbackRepeatedly;

using PaymentsRpcCardType =
    payments::PaymentsAutofillClient::PaymentsRpcCardType;
using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

class CreditCardAccessManagerMandatoryReauthTestBase
    : public CreditCardAccessManagerTestBase {
 public:
  CreditCardAccessManagerMandatoryReauthTestBase() = default;
  ~CreditCardAccessManagerMandatoryReauthTestBase() override = default;

 protected:
  void SetUp() override {
    CreditCardAccessManagerTestBase::SetUp();
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableFpanRiskBasedAuthentication);
#if BUILDFLAG(IS_ANDROID)
    if (base::android::device_info::is_automotive()) {
      autofill_client().GetPrefs()->SetBoolean(
          prefs::kAutofillPaymentMethodsMandatoryReauth,
          /*value=*/true);
      return;
    }
#endif  // BUILDFLAG(IS_ANDROID)
    autofill_client().GetPrefs()->SetBoolean(
        prefs::kAutofillPaymentMethodsMandatoryReauth,
        /*value=*/PrefIsEnabled());
  }

  void SetUpDeviceAuthenticatorResponseMock() {
    ON_CALL(mandatory_reauth_manager(), GetAuthenticationMethod)
        .WillByDefault(testing::Return(GetAuthenticationMethod()));

    // We should only expect an AuthenticateWithMessage() call if the feature
    // flag is on and the pref is enabled, or if the device is automotive.
    if (IsMandatoryReauthEnabled()) {
      ON_CALL(mandatory_reauth_manager(),
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
              AuthenticateWithMessage)
          .WillByDefault(RunOnceCallbackRepeatedly<1>(
#elif BUILDFLAG(IS_ANDROID)
              Authenticate)
          .WillByDefault(RunOnceCallbackRepeatedly<0>(
#endif
              MandatoryReauthResponseIsSuccess()));
    } else {
      EXPECT_CALL(mandatory_reauth_manager(),
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
                  AuthenticateWithMessage)
#elif BUILDFLAG(IS_ANDROID)
                  Authenticate)
#endif
          .Times(0);
    }
  }

  payments::MockMandatoryReauthManager& mandatory_reauth_manager() {
    return *static_cast<payments::MockMandatoryReauthManager*>(
        autofill_client()
            .GetPaymentsAutofillClient()
            ->GetOrCreatePaymentsMandatoryReauthManager());
  }

  virtual bool PrefIsEnabled() const = 0;

  virtual bool MandatoryReauthResponseIsSuccess() const = 0;

  virtual bool HasAuthenticator() const = 0;

  virtual payments::MandatoryReauthAuthenticationMethod
  GetAuthenticationMethod() const = 0;

  bool IsMandatoryReauthEnabled() {
#if BUILDFLAG(IS_ANDROID)
    if (base::android::device_info::is_automotive()) {
      return true;
    }
#endif
    return PrefIsEnabled();
  }

  base::test::ScopedFeatureList feature_list_;
};

// Parameters of the CreditCardAccessManagerMandatoryReauthFunctionalTest:
// - bool pref_is_enabled: Whether the mandatory re-auth pref is turned on or
// off.
// - bool mandatory_reauth_response_is_success: Whether the response from the
// mandatory re-auth is a success or failure.
// - bool authentication_method: The authentication method that is supported.
class CreditCardAccessManagerMandatoryReauthFunctionalTest
    : public CreditCardAccessManagerMandatoryReauthTestBase,
      public testing::WithParamInterface<
          std::tuple<bool,
                     bool,
                     payments::MandatoryReauthAuthenticationMethod>> {
 public:
  CreditCardAccessManagerMandatoryReauthFunctionalTest() = default;
  ~CreditCardAccessManagerMandatoryReauthFunctionalTest() override = default;

  bool PrefIsEnabled() const override { return std::get<0>(GetParam()); }

  bool MandatoryReauthResponseIsSuccess() const override {
    return std::get<1>(GetParam());
  }

  bool HasAuthenticator() const override {
    return std::get<2>(GetParam()) !=
           payments::MandatoryReauthAuthenticationMethod::kUnsupportedMethod;
  }

  payments::MandatoryReauthAuthenticationMethod GetAuthenticationMethod()
      const override {
    return std::get<2>(GetParam());
  }

  std::string GetStringForAuthenticationMethod() const {
    switch (GetAuthenticationMethod()) {
      case payments::MandatoryReauthAuthenticationMethod::kUnsupportedMethod:
        return ".UnsupportedMethod";
      case payments::MandatoryReauthAuthenticationMethod::kBiometric:
        return ".Biometric";
      case payments::MandatoryReauthAuthenticationMethod::kScreenLock:
        return ".ScreenLock";
      case payments::MandatoryReauthAuthenticationMethod::kUnknown:
        NOTIMPLEMENTED();
        return "";
    }
  }
};

// Tests that retrieving local cards works correctly in the context of the
// Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthFunctionalTest,
       MandatoryReauth_FetchLocalCard) {
  base::HistogramTester histogram_tester;
  CreateLocalCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  PrepareToFetchCreditCardAndWaitForCallbacks();

  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor().GetWeakPtr()));

  // The only time we should expect an error is if mandatory re-auth is
  // enabled, but the mandatory re-auth authentication was not successful.
  if (IsMandatoryReauthEnabled() && HasAuthenticator() &&
      !MandatoryReauthResponseIsSuccess()) {
    EXPECT_TRUE(accessor().number().empty());
  } else {
    EXPECT_EQ(accessor().number(), kTestNumber16);
    EXPECT_EQ(accessor().cvc(), kTestCvc16);
  }

  std::string reauth_usage_histogram_name =
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage.LocalCard";
  reauth_usage_histogram_name += GetStringForAuthenticationMethod();

  if (IsMandatoryReauthEnabled()) {
    if (HasAuthenticator()) {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowStarted,
          1);
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowSucceeded
              : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowFailed,
          1);
      histogram_tester.ExpectUniqueSample(
          "Autofill.ServerCardUnmask.LocalCard.Result.DeviceUnlock",
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::ServerCardUnmaskResult::
                    kAuthenticationUnmasked
              : autofill_metrics::ServerCardUnmaskResult::kAuthenticationError,
          1);
    } else {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowSkipped,
          1);
    }
  } else {
    histogram_tester.ExpectBucketCount(
        reauth_usage_histogram_name,
        autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
        0);
  }
}

// Tests that retrieving virtual cards works correctly in the context of the
// Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthFunctionalTest,
       MandatoryReauth_FetchVirtualCard) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, kTestServerId, kTestCvc16,
                   CreditCard::RecordType::kVirtualCard);
  const CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor().GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());

  const CreditCard* virtual_card_enrolled_regular_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  CreditCard virtual_card = *virtual_card_enrolled_regular_card;
  virtual_card.set_record_type(CreditCard::RecordType::kVirtualCard);
  virtual_card.set_cvc(u"321");
  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(virtual_card));

  // Ensure the accessor received the correct response.
  if (!IsMandatoryReauthEnabled() || !HasAuthenticator() ||
      MandatoryReauthResponseIsSuccess()) {
    EXPECT_EQ(accessor().number(), u"4234567890123456");
    EXPECT_EQ(accessor().cvc(), u"321");
    EXPECT_EQ(accessor().expiry_month(), base::UTF8ToUTF16(test::NextMonth()));
    EXPECT_EQ(accessor().expiry_year(), base::UTF8ToUTF16(test::NextYear()));
  }

  std::string reauth_usage_histogram_name =
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage.VirtualCard";
  reauth_usage_histogram_name += GetStringForAuthenticationMethod();

  if (IsMandatoryReauthEnabled()) {
    if (HasAuthenticator()) {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowStarted,
          1);
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowSucceeded
              : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowFailed,
          1);
      histogram_tester.ExpectUniqueSample(
          "Autofill.CvcStorage.CvcFilling.VirtualCard",
          autofill_metrics::CvcFillingFlowType::kMandatoryReauth,
          MandatoryReauthResponseIsSuccess() ? 1 : 0);
      histogram_tester.ExpectUniqueSample(
          "Autofill.ServerCardUnmask.VirtualCard.Result.DeviceUnlock",
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::ServerCardUnmaskResult::
                    kAuthenticationUnmasked
              : autofill_metrics::ServerCardUnmaskResult::kAuthenticationError,
          1);
    } else {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowSkipped,
          1);
    }
  } else {
    histogram_tester.ExpectBucketCount(
        reauth_usage_histogram_name,
        autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
        0);
  }
}

// Tests that retrieving masked server cards triggers mandatory reauth (if
// applicable) when risk-based auth returned the card.
TEST_P(CreditCardAccessManagerMandatoryReauthFunctionalTest,
       MandatoryReauth_FetchMaskedServerCard) {
  std::string test_number = "4444333322221111";
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, test_number, kTestServerId);
  const CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor().GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client()
                  .GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());

  // Mock CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse to
  // successfully return the valid card number.
  CreditCard card = *masked_server_card;
  card.set_record_type(CreditCard::RecordType::kFullServerCard);

  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(card));

  // Ensure the accessor received the correct response.
  if (!IsMandatoryReauthEnabled() || !HasAuthenticator() ||
      MandatoryReauthResponseIsSuccess()) {
    EXPECT_EQ(accessor().number(), base::UTF8ToUTF16(test_number));
  }
  std::string reauth_usage_histogram_name =
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage.ServerCard";
  reauth_usage_histogram_name += GetStringForAuthenticationMethod();

  if (IsMandatoryReauthEnabled()) {
    if (HasAuthenticator()) {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowStarted,
          1);
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowSucceeded
              : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowFailed,
          1);
      histogram_tester.ExpectUniqueSample(
          "Autofill.ServerCardUnmask.ServerCard.Result.DeviceUnlock",
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::ServerCardUnmaskResult::
                    kAuthenticationUnmasked
              : autofill_metrics::ServerCardUnmaskResult::kAuthenticationError,
          1);
    } else {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowSkipped,
          1);
    }
  } else {
    histogram_tester.ExpectBucketCount(
        reauth_usage_histogram_name,
        autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
        0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CreditCardAccessManagerMandatoryReauthFunctionalTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool(),
        testing::Values(
            payments::MandatoryReauthAuthenticationMethod::kUnsupportedMethod,
            payments::MandatoryReauthAuthenticationMethod::kBiometric,
            payments::MandatoryReauthAuthenticationMethod::kScreenLock)));

// Test suite built for testing mandatory re-auth's functionality as an
// integration with other projects.
// -- bool mandatory_reauth_response_is_success: Whether or not the re-auth
// authentication was successful.
class CreditCardAccessManagerMandatoryReauthIntegrationTest
    : public CreditCardAccessManagerMandatoryReauthTestBase,
      public testing::WithParamInterface<bool> {
 public:
  CreditCardAccessManagerMandatoryReauthIntegrationTest() = default;
  ~CreditCardAccessManagerMandatoryReauthIntegrationTest() override = default;

 protected:
  bool PrefIsEnabled() const override { return true; }

  bool MandatoryReauthResponseIsSuccess() const override { return GetParam(); }

  bool HasAuthenticator() const override { return true; }

  payments::MandatoryReauthAuthenticationMethod GetAuthenticationMethod()
      const override {
    return payments::MandatoryReauthAuthenticationMethod::kBiometric;
  }
};

// Tests that when retrieving local cards with a CVC stored, the CVC is filled.
// This test is in the context of the Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthIntegrationTest,
       MandatoryReauth_FetchLocalCard_CvcFillWorksCorrectly) {
  base::HistogramTester histogram_tester;
  CreateLocalCard(kTestGUID, kTestNumber);
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  PrepareToFetchCreditCardAndWaitForCallbacks();

  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor().GetWeakPtr()));

  EXPECT_EQ(accessor().cvc(),
            MandatoryReauthResponseIsSuccess() ? kTestCvc16 : u"");
  histogram_tester.ExpectBucketCount(
      "Autofill.CvcStorage.CvcFilling.LocalCard",
      autofill_metrics::CvcFillingFlowType::kMandatoryReauth,
      MandatoryReauthResponseIsSuccess() ? 1 : 0);
}

// Tests that when retrieving local cards without a CVC stored, the CVC is not
// filled. This test is in the context of the Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthIntegrationTest,
       MandatoryReauth_FetchLocalCard_NoCvcFillWorksCorrectly) {
  base::HistogramTester histogram_tester;
  CreateLocalCard(kTestGUID, kTestNumber, /*cvc=*/u"");
  const CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  PrepareToFetchCreditCardAndWaitForCallbacks();

  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor().GetWeakPtr()));

  EXPECT_EQ(accessor().cvc(), u"");
  histogram_tester.ExpectBucketCount(
      "Autofill.CvcStorage.CvcFilling.LocalCard",
      autofill_metrics::CvcFillingFlowType::kMandatoryReauth, 0);
}

// Tests that when retrieving masked server cards with a CVC stored, the CVC is
// filled. This test is in the context of the Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthIntegrationTest,
       MandatoryReauth_FetchMaskedServerCard_CvcFillWorksCorrectly) {
  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  CreateServerCard(kTestGUID, test_number, kTestServerId);
  const CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  PrepareToFetchCreditCardAndWaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor().GetWeakPtr()));

  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(*masked_server_card));

  EXPECT_EQ(accessor().cvc(),
            MandatoryReauthResponseIsSuccess() ? kTestCvc16 : u"");
  histogram_tester.ExpectBucketCount(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kMandatoryReauth,
      MandatoryReauthResponseIsSuccess() ? 1 : 0);
}

// Tests that when retrieving masked server cards without a CVC stored, the CVC
// is not filled. This test is in the context of the Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthIntegrationTest,
       MandatoryReauth_FetchMaskedServerCard_NoCvcFillWorksCorrectly) {
  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  CreateServerCard(kTestGUID, test_number, kTestServerId, /*cvc=*/u"");
  const CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  PrepareToFetchCreditCardAndWaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor().GetWeakPtr()));

  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(*masked_server_card));

  EXPECT_EQ(accessor().cvc(), u"");
  histogram_tester.ExpectBucketCount(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kMandatoryReauth, 0);
}

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerMandatoryReauthIntegrationTest,
                         testing::Bool());

}  // namespace
}  // namespace autofill
