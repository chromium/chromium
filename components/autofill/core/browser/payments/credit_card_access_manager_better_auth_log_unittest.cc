// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_base.h"
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"

// TODO(crbug.com/364261821): Extract other BetterAuth logging tests from
// credit_card_access_manager_unittest.cc to this unittest.

namespace autofill {
namespace {

// Params of the CreditCardAccessManagerBetterAuthLogTest:
// -- bool has_server_card;
// -- bool is_user_opted_in;
class CreditCardAccessManagerBetterAuthLogTest
    : public CreditCardAccessManagerTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CreditCardAccessManagerBetterAuthLogTest() = default;
  ~CreditCardAccessManagerBetterAuthLogTest() override = default;

  bool HasServerCard() { return std::get<0>(GetParam()); }
  bool IsUserOptedIn() { return std::get<1>(GetParam()); }

  void SetUp() override {
    ClearCards();
    if (HasServerCard()) {
      CreateServerCard(kTestGUID, kTestNumber);
    } else {
      CreateLocalCard(kTestGUID, kTestNumber);
    }
    CreditCardAccessManagerTestBase::SetUp();
  }

  const std::string kVerifiabilityCheckDurationMetrics =
      "Autofill.BetterAuth.UserVerifiabilityCheckDuration";
  const std::string kPreflightCallMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightCalledWithFidoOptInStatus";
  const std::string kPreflightLatencyMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightDuration";
  const std::string kPreflightFlowInitiatedMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightInitiated";
};

TEST_P(CreditCardAccessManagerBetterAuthLogTest,
       CardUnmaskPreflightCalledMetric_FidoEligible) {
  base::HistogramTester histogram_tester;
  auto* fido_authenticator = GetFIDOAuthenticator();
  fido_authenticator->SetUserVerifiable(/*is_user_verifiable=*/true);
  fido_authenticator->set_is_user_opted_in(IsUserOptedIn());
  ResetFetchCreditCard();
  credit_card_access_manager().PrepareToFetchCreditCard();
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();
  histogram_tester.ExpectTotalCount(/*name=*/kVerifiabilityCheckDurationMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
  if (HasServerCard()) {
    histogram_tester.ExpectUniqueSample(kPreflightCallMetrics,
                                        /*sample=*/IsUserOptedIn(),
                                        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(/*name=*/kPreflightCallMetrics,
                                      /*expected_count=*/0);
  }
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightCallMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightLatencyMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightFlowInitiatedMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
}

TEST_P(CreditCardAccessManagerBetterAuthLogTest,
       CardUnmaskPreflightCalledMetric_NotFidoEligible) {
  base::HistogramTester histogram_tester;
  GetFIDOAuthenticator()->SetUserVerifiable(/*is_user_verifiable=*/false);
  ResetFetchCreditCard();
  credit_card_access_manager().PrepareToFetchCreditCard();
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();
  if (HasServerCard()) {
    histogram_tester.ExpectUniqueSample(
        /*name=*/kPreflightFlowInitiatedMetrics, /*sample=*/true,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectTotalCount(
        /*name=*/kVerifiabilityCheckDurationMetrics,
        /*expected_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(/*name=*/kPreflightFlowInitiatedMetrics,
                                      /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        /*name=*/kVerifiabilityCheckDurationMetrics,
        /*expected_count=*/0);
  }
  histogram_tester.ExpectTotalCount(
      /*name=*/kPreflightCallMetrics,
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightLatencyMetrics,
                                    /*expected_count=*/0);
}

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerBetterAuthLogTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Params of the CreditCardAccessManagerBetterAuthOptInLogTest:
// -- bool is_virtual_card;
// -- bool unmask_details_offer_fido_opt_in;
// -- bool card_authorization_token_present;
// -- bool max_strikes_limit_reached;
// -- bool has_opted_in_from_android_settings;
// -- bool is_opted_in_for_fido;
class CreditCardAccessManagerBetterAuthOptInLogTest
    : public CreditCardAccessManagerTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, bool, bool, bool>> {
 public:
  CreditCardAccessManagerBetterAuthOptInLogTest() = default;
  ~CreditCardAccessManagerBetterAuthOptInLogTest() override = default;

  void SetUp() override {
    CreditCardAccessManagerTestBase::SetUp();

    if (MaxStrikesLimitReached()) {
      AddMaxStrikes();
    } else {
      ClearStrikes();
    }

    CreateServerCard(kTestGUID, kTestNumber);
    GetFIDOAuthenticator()->SetUserVerifiable(true);
#if BUILDFLAG(IS_ANDROID)
    SetCreditCardFIDOAuthEnabled(HasOptedInFromAndroidSettings());
#else
    SetCreditCardFIDOAuthEnabled(false);
#endif  // BUILDFLAG(OS_ANDROID)
    payments_network_interface().AllowFidoRegistration(
        /*offer_fido_opt_in=*/UnmaskDetailsOfferFidoOptIn());
    if (IsVirtualCard()) {
      GetCreditCard()->set_record_type(CreditCard::RecordType::kVirtualCard);
    }
    if (IsOptedIntoFido()) {
      // If user and device are already opted into FIDO, then add an eligible
      // card to ensure that the `unmask_details_` contains fido request
      // options.
      payments_network_interface().AddFidoEligibleCard(
          "random_id", kCredentialId, kGooglePaymentsRpid);
    }

    credit_card_access_manager().PrepareToFetchCreditCard();
    credit_card_access_manager().FetchCreditCard(
        GetCreditCard(), base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                        accessor_->GetWeakPtr()));
  }

  bool IsVirtualCard() { return std::get<0>(GetParam()); }
  bool UnmaskDetailsOfferFidoOptIn() { return std::get<1>(GetParam()); }
  bool CardAuthorizationTokenPresent() { return std::get<2>(GetParam()); }
  bool MaxStrikesLimitReached() { return std::get<3>(GetParam()); }
  bool HasOptedInFromAndroidSettings() { return std::get<4>(GetParam()); }
  bool IsOptedIntoFido() { return std::get<5>(GetParam()); }

  bool ShouldOfferFidoOptIn() {
    return !IsOptedIntoFido() && !IsVirtualCard() &&
           UnmaskDetailsOfferFidoOptIn() && CardAuthorizationTokenPresent() &&
           !MaxStrikesLimitReached();
  }

  bool ShouldOfferFidoOptInAndroid() {
    return !IsOptedIntoFido() && !IsVirtualCard() &&
           UnmaskDetailsOfferFidoOptIn() && !HasOptedInFromAndroidSettings();
  }

  const std::string GetFidoOptInNotOfferedHistogram() {
    return fido_opt_in_not_offered_histogram;
  }

  CreditCard* GetCreditCard() {
    return personal_data().payments_data_manager().GetCreditCardByGUID(
        kTestGUID);
  }

 private:
  const std::string fido_opt_in_not_offered_histogram =
      "Autofill.BetterAuth.OptInPromoNotOfferedReason";
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Ensures that the correct metrics are logged when the FIDO opt-in dialog is
// not shown on Desktop.
TEST_P(CreditCardAccessManagerBetterAuthOptInLogTest,
       FidoOptInNotShown_Desktop) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(test_api(credit_card_access_manager())
                .ShouldOfferFidoOptInDialog(
                    CreditCardCvcAuthenticator::CvcAuthenticationResponse()
                        .with_did_succeed(true)
                        .with_card(GetCreditCard())
                        .with_card_authorization_token(
                            CardAuthorizationTokenPresent()
                                ? "card_authorization_token"
                                : "")
                        .with_cvc(u"123")),
            ShouldOfferFidoOptIn());

  if (IsOptedIntoFido()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kAlreadyOptedIn,
        /*expected_bucket_count=*/1);
  } else if (!UnmaskDetailsOfferFidoOptIn()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kUnmaskDetailsOfferFidoOptInFalse,
        /*expected_bucket_count=*/1);
  } else if (!CardAuthorizationTokenPresent()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kCardAuthorizationTokenEmpty,
        /*expected_bucket_count=*/1);
  } else if (MaxStrikesLimitReached()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kBlockedByStrikeDatabase,
        /*expected_bucket_count=*/1);
  } else if (IsVirtualCard()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kVirtualCard,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(GetFidoOptInNotOfferedHistogram(),
                                      /*expected_count=*/0);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
// Ensures that the correct metrics are logged when the FIDO opt-in checkbox is
// not shown on Android.
TEST_P(CreditCardAccessManagerBetterAuthOptInLogTest,
       FidoOptInNotShown_Android) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(test_api(credit_card_access_manager()).ShouldOfferFidoAuth(),
            ShouldOfferFidoOptInAndroid());

  if (IsOptedIntoFido()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kAlreadyOptedIn,
        /*expected_bucket_count=*/1);
  } else if (!UnmaskDetailsOfferFidoOptIn()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kUnmaskDetailsOfferFidoOptInFalse,
        /*expected_bucket_count=*/1);
  } else if (HasOptedInFromAndroidSettings()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kOptedInFromSettings,
        /*expected_bucket_count=*/1);
  } else if (IsVirtualCard()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kVirtualCard,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(GetFidoOptInNotOfferedHistogram(),
                                      /*expected_count=*/0);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerBetterAuthOptInLogTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace
}  // namespace autofill
