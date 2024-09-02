// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_network_interface.h"

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface_test_base.h"
#include "components/autofill/core/browser/payments/test/autofill_payments_test_utils.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

using ::testing::HasSubstr;

using PaymentsRpcResult = PaymentsAutofillClient::PaymentsRpcResult;

struct CardUnmaskOptions {
  CardUnmaskOptions& with_fido() {
    use_fido = true;
    use_cvc = false;
    return *this;
  }

  CardUnmaskOptions& with_cvc(std::string c) {
    use_cvc = true;
    cvc = c;
    return *this;
  }

  CardUnmaskOptions& with_virtual_card() {
    virtual_card = true;
    return *this;
  }

  CardUnmaskOptions& with_virtual_card_risk_based() {
    with_virtual_card();
    use_cvc = false;
    return *this;
  }

  CardUnmaskOptions& with_virtual_card_risk_based_then_fido() {
    with_virtual_card();
    use_fido = true;
    use_cvc = false;
    set_context_token = true;
    return *this;
  }

  CardUnmaskOptions& with_virtual_card_risk_based_then_otp(std::string o) {
    with_virtual_card();
    use_otp = true;
    use_cvc = false;
    set_context_token = true;
    otp = o;
    return *this;
  }

  CardUnmaskOptions& with_only_non_legacy_id() {
    use_only_non_legacy_id = true;
    return *this;
  }

  CardUnmaskOptions& with_only_legacy_id() {
    use_only_legacy_id = true;
    return *this;
  }

  // By default, use cvc authentication.
  bool use_cvc = true;
  // If true, use FIDO authentication.
  bool use_fido = false;
  // If true, use otp authentication.
  bool use_otp = false;
  // If CVC authentication is chosen, default CVC value the user entered, to be
  // sent to Google Payments.
  std::string cvc = "123";
  // If OTP authentication is chosen, default OTP value the user entered.
  std::string otp = "654321";
  // If true, mock that the unmasking is for a virtual card.
  bool virtual_card = false;
  // If true, set context_token in the request.
  bool set_context_token = true;
  // If true, use only non-legacy instrument id.
  bool use_only_non_legacy_id = false;
  // If true, use only legacy instrument id.
  bool use_only_legacy_id = false;
};

class PaymentsNetworkInterfaceTest : public PaymentsNetworkInterfaceTestBase,
                                     public testing::Test {
 public:
  PaymentsNetworkInterfaceTest() = default;

  PaymentsNetworkInterfaceTest(const PaymentsNetworkInterfaceTest&) = delete;
  PaymentsNetworkInterfaceTest& operator=(const PaymentsNetworkInterfaceTest&) = delete;

  ~PaymentsNetworkInterfaceTest() override = default;

  void SetUp() override {
    SetUpTest();
    legal_message_.reset();
    payments_network_interface_ = std::make_unique<PaymentsNetworkInterface>(
        test_shared_loader_factory_, identity_test_env_.identity_manager(),
        &test_personal_data_.payments_data_manager());
  }

  void TearDown() override { payments_network_interface_.reset(); }

  void OnDidGetUnmaskDetails(
      PaymentsRpcResult result,
      payments::PaymentsNetworkInterface::UnmaskDetails& unmask_details) {
    result_ = result;
    unmask_details_ = unmask_details;
  }

  void OnDidGetRealPan(
      PaymentsRpcResult result,
      const PaymentsNetworkInterface::UnmaskResponseDetails& response) {
    result_ = result;
    unmask_response_details_ = response;
  }

  void OnDidGetOptChangeResult(
      PaymentsRpcResult result,
      PaymentsNetworkInterface::OptChangeResponseDetails& response) {
    result_ = result;
    opt_change_response_.user_is_opted_in = response.user_is_opted_in;
    opt_change_response_.fido_creation_options =
        std::move(response.fido_creation_options);
    opt_change_response_.fido_request_options =
        std::move(response.fido_request_options);
  }

  void OnDidGetUploadDetails(
      PaymentsRpcResult result,
      const std::u16string& context_token,
      std::unique_ptr<base::Value::Dict> legal_message,
      std::vector<std::pair<int, int>> supported_card_bin_ranges) {
    result_ = result;
    legal_message_ = std::move(legal_message);
    supported_card_bin_ranges_ = supported_card_bin_ranges;
  }

  void OnDidUploadCard(
      PaymentsRpcResult result,
      const PaymentsNetworkInterface::UploadCardResponseDetails&
          upload_card_respone_details) {
    result_ = result;
    upload_card_response_details_ = upload_card_respone_details;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void OnDidMigrateLocalCards(
      PaymentsRpcResult result,
      std::unique_ptr<std::unordered_map<std::string, std::string>>
          migration_save_results,
      const std::string& display_text) {
    result_ = result;
    migration_save_results_ = std::move(migration_save_results);
    display_text_ = display_text;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  void OnDidSelectChallengeOption(PaymentsRpcResult result,
                                  const std::string& updated_context_token) {
    result_ = result;
    context_token_ = updated_context_token;
  }

  void OnDidGetVirtualCardEnrollmentDetails(
      PaymentsRpcResult result,
      const payments::PaymentsNetworkInterface::
          GetDetailsForEnrollmentResponseDetails&
              get_details_for_enrollment_response_fields) {
    result_ = result;
    get_details_for_enrollment_response_fields_ =
        get_details_for_enrollment_response_fields;
  }

  void OnDidGetUpdateVirtualCardEnrollmentResponse(PaymentsRpcResult result) {
    result_ = result;
  }

 protected:
  std::unique_ptr<PaymentsNetworkInterface> payments_network_interface_;

  // Issue a GetUnmaskDetails request. This requires an OAuth token before
  // starting the request.
  void StartGettingUnmaskDetails() {
    payments_network_interface_->GetUnmaskDetails(
        base::BindOnce(&PaymentsNetworkInterfaceTest::OnDidGetUnmaskDetails,
                       GetWeakPtr()),
        "language-LOCALE");
  }

  // Issue an UnmaskCard request. This requires an OAuth token before starting
  // the request.
  void StartUnmasking(CardUnmaskOptions options) {
    PaymentsNetworkInterface::UnmaskRequestDetails request_details;
    request_details.billing_customer_number = 111222333444;

    request_details.card = options.use_only_non_legacy_id
                               ? test::GetMaskedServerCardWithNonLegacyId()
                               : options.use_only_legacy_id
                                     ? test::GetMaskedServerCardWithLegacyId()
                                     : test::GetMaskedServerCard();

    request_details.risk_data = "some risk data";
    if (options.use_fido) {
      request_details.fido_assertion_info = base::Value::Dict();
    }
    if (options.use_cvc)
      request_details.user_response.cvc = base::ASCIIToUTF16(options.cvc);
    if (options.virtual_card) {
      request_details.card.set_record_type(
          CreditCard::RecordType::kVirtualCard);
      request_details.last_committed_primary_main_frame_origin =
          GURL("https://www.example.com");
      if (options.use_cvc) {
        request_details.selected_challenge_option =
            test::GetCardUnmaskChallengeOptions(
                {CardUnmaskChallengeOptionType::kCvc})[0];
      }
    }
    if (options.set_context_token)
      request_details.context_token = "fake context token";
    if (options.use_otp)
      request_details.otp = base::ASCIIToUTF16(options.otp);
    payments_network_interface_->UnmaskCard(
        request_details,
        base::BindOnce(&PaymentsNetworkInterfaceTest::OnDidGetRealPan, GetWeakPtr()));
  }

  // If |opt_in| is set to true, then opts the user in to use FIDO
  // authentication for card unmasking. Otherwise opts the user out.
  void StartOptChangeRequest(
      PaymentsNetworkInterface::OptChangeRequestDetails::Reason reason) {
    PaymentsNetworkInterface::OptChangeRequestDetails request_details;
    request_details.reason = reason;
    payments_network_interface_->OptChange(
        request_details,
        base::BindOnce(&PaymentsNetworkInterfaceTest::OnDidGetOptChangeResult,
                       GetWeakPtr()));
  }

  // Issue a GetUploadDetails request. This may require an OAuth token before
  // starting the request.
  void StartGettingUploadDetails() {
    payments_network_interface_->GetCardUploadDetails(
        BuildTestProfiles(), /*detected_values=*/0,
        /*client_behavior_signals=*/{}, "language-LOCALE",
        base::BindOnce(&PaymentsNetworkInterfaceTest::OnDidGetUploadDetails,
                       GetWeakPtr()),
        /*billable_service_number=*/12345,
        /*billing_customer_number=*/111222333444L,
        /*upload_card_source=*/
        PaymentsNetworkInterface::UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE);
  }

  // Issue an UploadCard request. This requires an OAuth token before starting
  // the request.
  void StartUploading() {
    PaymentsNetworkInterface::UploadCardRequestDetails request_details;
    request_details.billing_customer_number = 111222333444L;
    request_details.card = test::GetCreditCard();
    request_details.context_token = u"context token";
    request_details.risk_data = "some risk data";
    request_details.app_locale = "language-LOCALE";
    request_details.profiles = BuildTestProfiles();
    payments_network_interface_->UploadCard(
        request_details,
        base::BindOnce(&PaymentsNetworkInterfaceTest::OnDidUploadCard, GetWeakPtr()));
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void StartMigrating() {
    PaymentsNetworkInterface::MigrationRequestDetails request_details;
    request_details.context_token = u"context token";
    request_details.risk_data = "some risk data";
    request_details.app_locale = "language-LOCALE";

    migratable_credit_cards_.clear();
    migratable_credit_cards_.emplace_back(test::GetCreditCard());
    migratable_credit_cards_.emplace_back(test::GetCreditCard2());
    payments_network_interface_->MigrateCards(
        request_details, migratable_credit_cards_,
        base::BindOnce(&PaymentsNetworkInterfaceTest::OnDidMigrateLocalCards,
                       GetWeakPtr()));
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  void StartSelectingChallengeOption(
      CardUnmaskChallengeOptionType challenge_type =
          CardUnmaskChallengeOptionType::kSmsOtp,
      std::string challenge_id = "arbitrary id") {
    PaymentsNetworkInterface::SelectChallengeOptionRequestDetails request_details;
    request_details.billing_customer_number = 555666777888;
    request_details.context_token = "fake context token";

    CardUnmaskChallengeOption selected_challenge_option;
    selected_challenge_option.type = challenge_type;
    selected_challenge_option.id =
        CardUnmaskChallengeOption::ChallengeOptionId(challenge_id);
    selected_challenge_option.challenge_info = u"(***)-***-5678";
    request_details.selected_challenge_option = selected_challenge_option;

    payments_network_interface_->SelectChallengeOption(
        request_details,
        base::BindOnce(&PaymentsNetworkInterfaceTest::OnDidSelectChallengeOption,
                       GetWeakPtr()));
  }

  void assertCvcIncludedInRequest(std::string cvc) {
    EXPECT_TRUE(!GetUploadData().empty());
    // Verify that the encrypted_cvc and s7e_13_cvc parameters were both
    // included in the request.
    EXPECT_TRUE(GetUploadData().find("encrypted_cvc") != std::string::npos);
    EXPECT_TRUE(GetUploadData().find("__param:s7e_13_cvc") !=
                std::string::npos);
    EXPECT_TRUE(GetUploadData().find("&s7e_13_cvc=" + cvc) !=
                std::string::npos);
  }

  void assertOtpIncludedInRequest(std::string otp) {
    EXPECT_TRUE(!GetUploadData().empty());
    // Verify that the otp and s7e_263_otp parameters were both included in the
    // request.
    EXPECT_TRUE(GetUploadData().find("otp") != std::string::npos);
    EXPECT_TRUE(GetUploadData().find("__param:s7e_263_otp") !=
                std::string::npos);
    EXPECT_TRUE(GetUploadData().find("&s7e_263_otp=" + otp) !=
                std::string::npos);
  }

  void assertCvcNotIncludedInRequest() {
    EXPECT_TRUE(!GetUploadData().empty());
    // Verify that the encrypted_cvc and s7e_13_cvc parameters were NOT included
    // in the request.
    EXPECT_TRUE(GetUploadData().find("encrypted_cvc") == std::string::npos);
    EXPECT_TRUE(GetUploadData().find("__param:s7e_13_cvc") ==
                std::string::npos);
    EXPECT_TRUE(GetUploadData().find("&s7e_13_cvc=") == std::string::npos);
  }

  void assertOtpNotIncludedInRequest() {
    EXPECT_TRUE(!GetUploadData().empty());
    // Verify that the otp and s7e_263_otp parameters were NOT included in the
    // request.
    EXPECT_TRUE(GetUploadData().find("otp") == std::string::npos);
    EXPECT_TRUE(GetUploadData().find("__param:s7e_263_otp") ==
                std::string::npos);
    EXPECT_TRUE(GetUploadData().find("&s7e_263_otp=") == std::string::npos);
  }

  void assertIncludedInRequest(std::string field_name_or_value) {
    EXPECT_TRUE(GetUploadData().find(field_name_or_value) != std::string::npos);
  }

  void assertNotIncludedInRequest(std::string field_name_or_value) {
    EXPECT_TRUE(GetUploadData().find(field_name_or_value) == std::string::npos);
  }

  const PaymentsNetworkInterface::UnmaskDetails* unmask_details() const {
    return unmask_details_ ? &unmask_details_.value() : nullptr;
  }
  const PaymentsNetworkInterface::UnmaskResponseDetails* unmask_response_details() const {
    return unmask_response_details_ ? &unmask_response_details_.value()
                                    : nullptr;
  }
  void ResetUnmaskResponseDetails() { unmask_response_details_.reset(); }

  base::WeakPtr<PaymentsNetworkInterfaceTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  PaymentsRpcResult result_ = PaymentsRpcResult::kNone;

  // Server ID of a saved card via credit card upload save.
  PaymentsNetworkInterface::UploadCardResponseDetails upload_card_response_details_;
  // The OptChangeResponseDetails retrieved from an OptChangeRequest.
  PaymentsNetworkInterface::OptChangeResponseDetails opt_change_response_;
  // The response details retrieved from an GetDetailsForEnrollmentRequest.
  PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails
      get_details_for_enrollment_response_fields_;
  // The legal message returned from a GetDetails upload save preflight call.
  std::unique_ptr<base::Value::Dict> legal_message_;
  // A list of card BIN ranges supported by Google Payments, returned from a
  // GetDetails upload save preflight call.
  std::vector<std::pair<int, int>> supported_card_bin_ranges_;
  // The opaque token used to chain consecutive payments requests together.
  std::string context_token_;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Credit cards to be upload saved during a local credit card migration call.
  std::vector<MigratableCreditCard> migratable_credit_cards_;
  // A mapping of results from a local credit card migration call.
  std::unique_ptr<std::unordered_map<std::string, std::string>>
      migration_save_results_;
  // A tip message to be displayed during local card migration.
  std::string display_text_;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

 private:
  std::optional<PaymentsNetworkInterface::UnmaskDetails> unmask_details_;
  // The UnmaskResponseDetails retrieved from an UnmaskRequest.  Includes PAN.
  std::optional<PaymentsNetworkInterface::UnmaskResponseDetails>
      unmask_response_details_;
  base::WeakPtrFactory<PaymentsNetworkInterfaceTest> weak_ptr_factory_{this};
};

TEST_F(PaymentsNetworkInterfaceTest, GetUnmaskDetailsSuccess) {
  StartGettingUnmaskDetails();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"offer_fido_opt_in\": \"false\", "
                 "\"authentication_method\": \"CVC\" }");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ(false, unmask_details()->offer_fido_opt_in);
  EXPECT_EQ(PaymentsAutofillClient::UnmaskAuthMethod::kCvc,
            unmask_details()->unmask_auth_method);
}

TEST_F(PaymentsNetworkInterfaceTest, GetUnmaskDetailsIncludesChromeUserContext) {
  StartGettingUnmaskDetails();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsNetworkInterfaceTest, OAuthError) {
  StartUnmasking(CardUnmaskOptions());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_TRUE(unmask_response_details()->real_pan.empty());
}

TEST_F(PaymentsNetworkInterfaceTest,
       UnmaskRequestIncludesBillingCustomerNumberInRequest) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();

  // Verify that the billing customer number is included in the request.
  EXPECT_TRUE(
      GetUploadData().find("%22external_customer_id%22:%22111222333444%22") !=
      std::string::npos);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskSuccessViaCVC) {
  StartUnmasking(CardUnmaskOptions().with_cvc("111"));
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"1234\" }");

  assertCvcIncludedInRequest("111");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("1234", unmask_response_details()->real_pan);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskSuccessViaFIDO) {
  StartUnmasking(CardUnmaskOptions().with_fido());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"1234\" }");

  assertCvcNotIncludedInRequest();
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("1234", unmask_response_details()->real_pan);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskSuccessViaCVCWithCreationOptions) {
  StartUnmasking(CardUnmaskOptions().with_cvc("111"));
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"1234\", \"dcvv\": \"321\"}");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("1234", unmask_response_details()->real_pan);
  EXPECT_EQ("321", unmask_response_details()->dcvv);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskSuccessAccountFromSyncTest) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"1234\" }");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("1234", unmask_response_details()->real_pan);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskSuccessWithVirtualCardCvcAuth) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card().with_cvc("222"));
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\",  "
                 "\"expiration\": { \"month\":12, \"year\":2099 } }");

  assertCvcIncludedInRequest("222");
  assertIncludedInRequest("cvc_challenge_option");
  assertIncludedInRequest("challenge_id");
  assertIncludedInRequest("cvc_length");
  assertIncludedInRequest("cvc_position");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskSuccessWithVirtualCardFidoAuth) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card().with_fido());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\",  "
                 "\"expiration\": { \"month\":12, \"year\":2099 } }");

  assertCvcNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
}

TEST_F(PaymentsNetworkInterfaceTest, VirtualCardRiskBasedGreenPathResponse) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\",  "
                 "\"expiration\": { \"month\":12, \"year\":2099 } }");

  // Verify that Cvc/FIDO/OTP are not included in the request.
  assertCvcNotIncludedInRequest();
  assertOtpNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  EXPECT_TRUE(GetUploadData().find("fido_assertion_info") == std::string::npos);
  // Only merchant_domain is included.
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
  EXPECT_TRUE(unmask_response_details()->card_unmask_challenge_options.empty());
}

TEST_F(PaymentsNetworkInterfaceTest, VirtualCardRiskBasedRedPathResponse_Error) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"error\": { \"code\": \"NON-INTERNAL\", "
                 "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
  EXPECT_EQ(PaymentsRpcResult::kVcnRetrievalPermanentFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest,
       VirtualCardRiskBasedRedPathResponse_NoOptionProvided) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"context_token\": \"fake_context_token\" }");
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, VirtualCardRiskBasedYellowPathResponse) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{ \"fido_request_options\": { \"challenge\": \"fake_fido_challenge\" }, "
      "\"context_token\": \"fake_context_token\", \"idv_challenge_options\": "
      "[{ \"sms_otp_challenge_option\": { \"challenge_id\": "
      "\"fake_challenge_id_1\", \"masked_phone_number\": \"(***)-***-1234\" } "
      "}, { \"sms_otp_challenge_option\": { \"challenge_id\": "
      "\"fake_challenge_id_2\", \"masked_phone_number\": \"(***)-***-5678\" } "
      "}, { \"cvc_challenge_option\": { \"challenge_id\": "
      "\"fake_challenge_id_3\", \"cvc_length\": 3, \"cvc_position\": "
      "\"CVC_POSITION_BACK\"}}]}");

  // Ensure that it's not treated as failure when no pan is returned.
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("fake_context_token", unmask_response_details()->context_token);
  // Verify the FIDO request challenge is correctly parsed.
  EXPECT_EQ(
      "fake_fido_challenge",
      *unmask_response_details()->fido_request_options.FindString("challenge"));
  // Verify the three challenge options are two sms challenge options and one
  // cvc challenge option, and fields can be correctly parsed.
  ASSERT_EQ(3u,
            unmask_response_details()->card_unmask_challenge_options.size());
  const CardUnmaskChallengeOption& challenge_option_1 =
      unmask_response_details()->card_unmask_challenge_options[0];
  EXPECT_EQ(CardUnmaskChallengeOptionType::kSmsOtp, challenge_option_1.type);
  EXPECT_EQ("fake_challenge_id_1", challenge_option_1.id.value());
  EXPECT_EQ(u"(***)-***-1234", challenge_option_1.challenge_info);
  const CardUnmaskChallengeOption& challenge_option_2 =
      unmask_response_details()->card_unmask_challenge_options[1];
  EXPECT_EQ(CardUnmaskChallengeOptionType::kSmsOtp, challenge_option_2.type);
  EXPECT_EQ("fake_challenge_id_2", challenge_option_2.id.value());
  EXPECT_EQ(u"(***)-***-5678", challenge_option_2.challenge_info);
  const CardUnmaskChallengeOption& challenge_option_3 =
      unmask_response_details()->card_unmask_challenge_options[2];
  EXPECT_EQ(CardUnmaskChallengeOptionType::kCvc, challenge_option_3.type);
  EXPECT_EQ("fake_challenge_id_3", challenge_option_3.id.value());
  EXPECT_EQ(challenge_option_3.challenge_info,
            u"This is the 3-digit code on the back of your card");
  EXPECT_EQ(3u, challenge_option_3.challenge_input_length);
  EXPECT_EQ(CvcPosition::kBackOfCard, challenge_option_3.cvc_position);
}

TEST_F(PaymentsNetworkInterfaceTest, VirtualCardCvcRetrieval_FlowStatusPresent) {
  StartUnmasking(
      CardUnmaskOptions().with_virtual_card_risk_based().with_cvc("123"));
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{\"flow_status\": \"FLOW_STATUS_INCORRECT_ACCOUNT_SECURITY_CODE\"}");

  // Ensure that it is treated as a try again failure when a flow status is
  // returned.
  EXPECT_EQ(PaymentsRpcResult::kTryAgainFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest,
       VirtualCardRiskBasedYellowPathResponseWithUnknownType) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{ \"fido_request_options\": { \"challenge\": \"fake_fido_challenge\" }, "
      "\"context_token\": \"fake_context_token\", \"idv_challenge_options\": "
      "[{ \"sms_otp_challenge_option\": { \"challenge_id\": "
      "\"fake_challenge_id_1\", \"masked_phone_number\": \"(***)-***-1234\" } "
      "}, { \"unknown_new_challenge_option\": { \"challenge_id\": "
      "\"fake_challenge_id_2\" } }] }");

  // Ensure that it's not treated as failure when no pan is returned.
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("fake_context_token", unmask_response_details()->context_token);
  // Verify the FIDO request challenge is correctly parsed.
  EXPECT_EQ(
      "fake_fido_challenge",
      *unmask_response_details()->fido_request_options.FindString("challenge"));
  // Verify that the unknow new challenge option type won't break the parsing.
  // We ignore the unknown new type, and only return the supported challenge
  // option.
  EXPECT_EQ(1u,
            unmask_response_details()->card_unmask_challenge_options.size());
  const CardUnmaskChallengeOption& sms_challenge_option =
      unmask_response_details()->card_unmask_challenge_options[0];
  EXPECT_EQ(CardUnmaskChallengeOptionType::kSmsOtp, sms_challenge_option.type);
  EXPECT_EQ("fake_challenge_id_1", sms_challenge_option.id.value());
  EXPECT_EQ(u"(***)-***-1234", sms_challenge_option.challenge_info);
}

TEST_F(PaymentsNetworkInterfaceTest, VirtualCardRiskBasedThenFido) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based_then_fido());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\",  "
                 "\"expiration\": { \"month\":12, \"year\":2099 } }");

  // Verify that Cvc/OTP are not included in the request.
  assertCvcNotIncludedInRequest();
  assertOtpNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  // Verify the fido assertion and context token is included.
  EXPECT_TRUE(GetUploadData().find("fido_assertion_info") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("context_token") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
}

TEST_F(PaymentsNetworkInterfaceTest, VirtualCardRiskBasedThenOtpSuccess) {
  const std::string otp = "111111";
  StartUnmasking(
      CardUnmaskOptions().with_virtual_card_risk_based_then_otp(otp));
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\",  "
                 "\"expiration\": { \"month\":12, \"year\":2099 } }");

  assertOtpIncludedInRequest(otp);
  // Verify that Cvc/FIDO are not included in the request.
  assertCvcNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  EXPECT_TRUE(GetUploadData().find("fido_assertion_info") == std::string::npos);
  // Verify the context token is also included.
  EXPECT_TRUE(GetUploadData().find("context_token") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
}

TEST_F(PaymentsNetworkInterfaceTest, ExpiredOtp) {
  const std::string otp = "222222";
  StartUnmasking(
      CardUnmaskOptions().with_virtual_card_risk_based_then_otp(otp));
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"context_token\": \"fake_context_token\", "
                 "\"flow_status\": \"FLOW_STATUS_EXPIRED_OTP\" }");

  assertOtpIncludedInRequest(otp);
  assertCvcNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  // Verify the context token is also included.
  EXPECT_TRUE(GetUploadData().find("context_token") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("FLOW_STATUS_EXPIRED_OTP", unmask_response_details()->flow_status);
}

TEST_F(PaymentsNetworkInterfaceTest, IncorrectOtp) {
  const std::string otp = "333333";
  StartUnmasking(
      CardUnmaskOptions().with_virtual_card_risk_based_then_otp(otp));
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"context_token\": \"fake_context_token\", "
                 "\"flow_status\": \"FLOW_STATUS_INCORRECT_OTP\" }");

  assertOtpIncludedInRequest(otp);
  assertCvcNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  // Verify the context token is also included.
  EXPECT_TRUE(GetUploadData().find("context_token") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("FLOW_STATUS_INCORRECT_OTP",
            unmask_response_details()->flow_status);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskIncludesLegacyAndNonLegacyId) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");

  // Non-legacy Instrument id and legacy server id are both set.
  EXPECT_TRUE(GetUploadData().find("%22instrument_id%22:%221%22") !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("%22credit_card_id%22:%22a123%22") !=
              std::string::npos);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskIncludesOnlyLegacyId) {
  StartUnmasking(CardUnmaskOptions().with_only_legacy_id());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");

  // Only legacy server id is set.
  EXPECT_TRUE(GetUploadData().find("instrument_id") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("%22credit_card_id%22:%22a123%22") !=
              std::string::npos);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskIncludesOnlyNonLegacyId) {
  StartUnmasking(CardUnmaskOptions().with_only_non_legacy_id());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");

  // Only non-legacy instrument id is set.
  EXPECT_TRUE(GetUploadData().find("%22instrument_id%22:%221%22") !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("credit_card_id") == std::string::npos);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskIncludesChromeUserContext) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskIncludesMerchantDomain) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");

  // last_committed_primary_main_frame_origin was set.
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskResponseIncludesDeclineDetails) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{\"error\": {\"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_temporary_error\"}, "
                 "\"decline_details\": {\"user_message_title\": "
                 "\"test_user_message_title\", \"user_message_description\": "
                 "\"test_user_message_description\"}}");

  EXPECT_EQ(PaymentsRpcResult::kVcnRetrievalTryAgainFailure, result_);
  EXPECT_TRUE(
      unmask_response_details()->autofill_error_dialog_context.has_value());
  AutofillErrorDialogContext autofill_error_dialog_context =
      *unmask_response_details()->autofill_error_dialog_context;
  EXPECT_EQ(*autofill_error_dialog_context.server_returned_title,
            "test_user_message_title");
  EXPECT_EQ(*autofill_error_dialog_context.server_returned_description,
            "test_user_message_description");
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskResponseIncludesEmptyDeclineDetails) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{\"error\": {\"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_temporary_error\"}, "
                 "\"decline_details\": {\"user_message_title\": "
                 "\"\", \"user_message_description\": "
                 "\"\"}}");

  EXPECT_EQ(PaymentsRpcResult::kVcnRetrievalTryAgainFailure, result_);
  EXPECT_FALSE(
      unmask_response_details()->autofill_error_dialog_context.has_value());
}

TEST_F(PaymentsNetworkInterfaceTest, OptInSuccess) {
  StartOptChangeRequest(
      PaymentsNetworkInterface::OptChangeRequestDetails::ENABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"fido_authentication_info\": { \"user_status\": "
                 "\"FIDO_AUTH_ENABLED\"}}");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_TRUE(opt_change_response_.user_is_opted_in.value());
}

TEST_F(PaymentsNetworkInterfaceTest, OptInServerUnresponsive) {
  StartOptChangeRequest(
      PaymentsNetworkInterface::OptChangeRequestDetails::ENABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_REQUEST_TIMEOUT,
                 "");
  EXPECT_EQ(PaymentsRpcResult::kNetworkError, result_);
  EXPECT_FALSE(opt_change_response_.user_is_opted_in.has_value());
}

TEST_F(PaymentsNetworkInterfaceTest, OptOutSuccess) {
  StartOptChangeRequest(
      PaymentsNetworkInterface::OptChangeRequestDetails::DISABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"fido_authentication_info\": { \"user_status\": "
                 "\"FIDO_AUTH_DISABLED\"}}");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_FALSE(opt_change_response_.user_is_opted_in.value());
}

TEST_F(PaymentsNetworkInterfaceTest, EnrollAttemptReturnsCreationOptions) {
  StartOptChangeRequest(
      PaymentsNetworkInterface::OptChangeRequestDetails::ENABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"fido_authentication_info\": { \"user_status\": "
                 "\"FIDO_AUTH_DISABLED\","
                 "\"fido_creation_options\": {"
                 "\"relying_party_id\": \"google.com\"}}}");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_FALSE(opt_change_response_.user_is_opted_in.value());
  EXPECT_EQ("google.com",
            *opt_change_response_.fido_creation_options->FindString(
                "relying_party_id"));
}

TEST_F(PaymentsNetworkInterfaceTest, GetDetailsSuccess) {
  StartGettingUploadDetails();
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_NE(nullptr, legal_message_.get());
}

TEST_F(PaymentsNetworkInterfaceTest, GetDetailsSuccessRequestLatencyMetric) {
  base::HistogramTester histogram_tester;

  StartGettingUploadDetails();
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");

  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentsNetworkInterface.RequestLatency.GetCardUploadDetails",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentsNetworkInterface.RequestLatency.GetCardUploadDetails."
      "Success",
      1);
}

TEST_F(PaymentsNetworkInterfaceTest, GetDetailsFailureRequestLatencyMetric) {
  base::HistogramTester histogram_tester;

  StartGettingUploadDetails();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"error\": { \"code\": \"INTERNAL\" }, \"context_token\": "
                 "\"some_token\", \"legal_message\": {} }");

  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentsNetworkInterface.RequestLatency.GetCardUploadDetails",
      1);
  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentsNetworkInterface.RequestLatency.GetCardUploadDetails."
      "Failure",
      1);
}

TEST_F(PaymentsNetworkInterfaceTest, GetUploadDetailsVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartGettingUploadDetails();
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());
}

TEST_F(PaymentsNetworkInterfaceTest, GetDetailsFollowedByUploadSuccess) {
  StartGettingUploadDetails();
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);

  result_ = PaymentsRpcResult::kNone;

  StartUploading();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, GetDetailsMissingContextToken) {
  StartGettingUploadDetails();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"legal_message\": {} }");
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, GetDetailsMissingLegalMessage) {
  StartGettingUploadDetails();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"context_token\": \"some_token\" }");
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ(nullptr, legal_message_.get());
}

TEST_F(PaymentsNetworkInterfaceTest, SupportedCardBinRangesParsesCorrectly) {
  StartGettingUploadDetails();
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{"
      "  \"context_token\" : \"some_token\","
      "  \"legal_message\" : {},"
      "  \"supported_card_bin_ranges_string\" : \"1234,300000-555555,765\""
      "}");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  // Check that |supported_card_bin_ranges_| has the two entries specified in
  // ReturnResponse(payments_network_interface_.get(),~) above.
  ASSERT_EQ(3U, supported_card_bin_ranges_.size());
  EXPECT_EQ(1234, supported_card_bin_ranges_[0].first);
  EXPECT_EQ(1234, supported_card_bin_ranges_[0].second);
  EXPECT_EQ(300000, supported_card_bin_ranges_[1].first);
  EXPECT_EQ(555555, supported_card_bin_ranges_[1].second);
  EXPECT_EQ(765, supported_card_bin_ranges_[2].first);
  EXPECT_EQ(765, supported_card_bin_ranges_[2].second);
}

TEST_F(PaymentsNetworkInterfaceTest, GetUploadAccountFromSyncTest) {
  // Set up a different account.
  const AccountInfo& secondary_account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");
  test_personal_data_.test_payments_data_manager().SetAccountInfoForPayments(
      secondary_account_info);

  StartUploading();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");

  // Issue a token for the secondary account.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      secondary_account_info.account_id, "secondary_account_token",
      AutofillClock::Now() + base::Days(10));

  // Verify the auth header.
  EXPECT_THAT(
      intercepted_headers_.GetHeader(net::HttpRequestHeaders::kAuthorization),
      testing::Optional(std::string("Bearer secondary_account_token")))
      << intercepted_headers_.ToString();
}

TEST_F(PaymentsNetworkInterfaceTest, UploadCardVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartUploading();
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskCardVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());
}

TEST_F(PaymentsNetworkInterfaceTest, UploadSuccessEmptyResponse) {
  StartUploading();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_FALSE(upload_card_response_details_.instrument_id.has_value());
  EXPECT_TRUE(upload_card_response_details_.virtual_card_enrollment_state ==
              CreditCard::VirtualCardEnrollmentState::kUnspecified);
  EXPECT_TRUE(upload_card_response_details_.card_art_url.is_empty());
}

TEST_F(PaymentsNetworkInterfaceTest, UploadSuccessInstrumentIdPresent) {
  StartUploading();
  IssueOAuthToken();
  upload_card_response_details_.instrument_id = std::nullopt;

  // Test the conversion from string to int64_t using the max value for int64_t.
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"instrument_id\": \"9223372036854775807\" }");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ(upload_card_response_details_.instrument_id, 9223372036854775807);
}

TEST_F(PaymentsNetworkInterfaceTest, UploadSuccessVirtualCardEnrollmentStatePresent) {
  bool oauth_token_issued = false;
  for (CreditCard::VirtualCardEnrollmentState virtual_card_enrollment_state :
       {CreditCard::VirtualCardEnrollmentState::kUnenrolledAndNotEligible,
        CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible,
        CreditCard::VirtualCardEnrollmentState::kEnrolled}) {
    StartUploading();
    // An OAuthToken needs to be issued to initiate the first UploadCard call
    // from PaymentsNetworkInterfaceTest::StartUploading(), but only for the first call.
    // All future calls will use the first OAuthToken. If multiple OAuthTokens
    // are issued this test will time out.
    if (!oauth_token_issued) {
      IssueOAuthToken();
      oauth_token_issued = true;
    }
    switch (virtual_card_enrollment_state) {
      case CreditCard::VirtualCardEnrollmentState::kUnenrolledAndNotEligible:
        ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                       "{ \"virtual_card_metadata\": { \"status\": "
                       "\"ENROLLMENT_STATUS_UNSPECIFIED\" } }");
        break;
      case CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible:
        ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                       "{ \"virtual_card_metadata\": { \"status\": "
                       "\"ENROLLMENT_ELIGIBLE\" } }");
        break;
      case CreditCard::VirtualCardEnrollmentState::kEnrolled:
        ReturnResponse(
            payments_network_interface_.get(), net::HTTP_OK,
            "{ \"virtual_card_metadata\": { \"status\": \"ENROLLED\" } }");
        break;
      case CreditCard::VirtualCardEnrollmentState::kUnenrolled:
      case CreditCard::VirtualCardEnrollmentState::kUnspecified:
        break;
    }
    EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
    EXPECT_EQ(upload_card_response_details_.virtual_card_enrollment_state,
              virtual_card_enrollment_state);
  }
}

TEST_F(PaymentsNetworkInterfaceTest,
       UploadSuccessGetDetailsForEnrollmentResponseDetailsPresent) {
  StartUploading();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"virtual_card_metadata\": "
                 "{\"status\": \"ENROLLMENT_ELIGIBLE\", "
                 "\"virtual_card_enrollment_data\": { "
                 "\"google_legal_message\": { \"line\" : [{ "
                 "\"template\": \"This is the entire message.\" }] }, "
                 "\"external_legal_message\": {},"
                 "\"context_token\": \"some_token\"} } }");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ(upload_card_response_details_.virtual_card_enrollment_state,
            CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible);
  EXPECT_EQ(
      upload_card_response_details_.get_details_for_enrollment_response_details
          .value()
          .google_legal_message[0]
          .text(),
      u"This is the entire message.");
  EXPECT_TRUE(
      upload_card_response_details_.get_details_for_enrollment_response_details
          .value()
          .issuer_legal_message.empty());
  EXPECT_EQ(
      upload_card_response_details_.get_details_for_enrollment_response_details
          .value()
          .vcn_context_token,
      "some_token");
}

TEST_F(PaymentsNetworkInterfaceTest, UploadSuccessCardArtUrlPresent) {
  StartUploading();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"card_art_url\": \"https://www.example.com/\" }");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ(upload_card_response_details_.card_art_url.spec(),
            "https://www.example.com/");
}

TEST_F(PaymentsNetworkInterfaceTest, UploadSuccessMeasureTimeoutHistogram) {
  base::HistogramTester histogram_tester;

  base::FieldTrialParams params;
  params["autofill_upload_card_request_timeout_milliseconds"] = "10000";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillUploadCardRequestTimeout, params);

  StartUploading();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsNetworkInterface.UploadCardRequest.ClientSideTimedOut",
      /*sample=*/false, /*expected_bucket_count=*/1);
}

TEST_F(PaymentsNetworkInterfaceTest, UploadFailureDueToClientSideTimeout) {
  base::HistogramTester histogram_tester;

  base::FieldTrialParams params;
  params["autofill_upload_card_request_timeout_milliseconds"] = "10000";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillUploadCardRequestTimeout, params);

  // Fake a client-side timeout on the card upload.
  StartUploading();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::ERR_TIMED_OUT, "");

  EXPECT_EQ(PaymentsRpcResult::kClientSideTimeout, result_);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsNetworkInterface.UploadCardRequest.ClientSideTimedOut",
      /*sample=*/true, /*expected_bucket_count=*/1);
}

TEST_F(PaymentsNetworkInterfaceTest,
       UploadClientTimeoutNotRecordedForOtherFailure) {
  base::HistogramTester histogram_tester;

  base::FieldTrialParams params;
  params["autofill_upload_card_request_timeout_milliseconds"] = "10000";
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kAutofillUploadCardRequestTimeout, params);

  // Fake a network issue on the upload; this shouldn't result in any record
  // being made for the client timeout histogram. In particular,
  // HTTP_REQUEST_TIMEOUT is treated differently than the client side timeout.
  StartUploading();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_REQUEST_TIMEOUT,
                 "");

  EXPECT_EQ(PaymentsRpcResult::kNetworkError, result_);
  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentsNetworkInterface.UploadCardRequest.ClientSideTimedOut",
      /*expected_count=*/0);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskMissingPan) {
  StartUnmasking(CardUnmaskOptions());
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskRetryFailure) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"error\": { \"code\": \"INTERNAL\" } }");
  EXPECT_EQ(PaymentsRpcResult::kTryAgainFailure, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskPermanentFailure) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskMalformedResponse) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"error_code\": \"WRONG_JSON_FORMAT\" }");
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsNetworkInterfaceTest, ReauthNeeded) {
  {
    StartUnmasking(CardUnmaskOptions());
    IssueOAuthToken();
    ReturnResponse(payments_network_interface_.get(), net::HTTP_UNAUTHORIZED,
                   "");
    // No response yet.
    EXPECT_EQ(PaymentsRpcResult::kNone, result_);
    EXPECT_EQ(nullptr, unmask_response_details());

    // Second HTTP_UNAUTHORIZED causes permanent failure.
    IssueOAuthToken();
    ReturnResponse(payments_network_interface_.get(), net::HTTP_UNAUTHORIZED,
                   "");
    EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
    EXPECT_EQ("", unmask_response_details()->real_pan);
  }

  result_ = PaymentsRpcResult::kNone;
  ResetUnmaskResponseDetails();

  {
    StartUnmasking(CardUnmaskOptions());
    // NOTE: Don't issue an access token here: the issuing of an access token
    // first waits for the access token request to be received, but here there
    // should be no access token request because PaymentsNetworkInterface should reuse the
    // access token from the previous request.
    ReturnResponse(payments_network_interface_.get(), net::HTTP_UNAUTHORIZED,
                   "");
    // No response yet.
    EXPECT_EQ(PaymentsRpcResult::kNone, result_);
    EXPECT_EQ(nullptr, unmask_response_details());

    // HTTP_OK after first HTTP_UNAUTHORIZED results in success.
    IssueOAuthToken();
    ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                   "{ \"pan\": \"1234\" }");
    EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
    EXPECT_EQ("1234", unmask_response_details()->real_pan);
  }
}

TEST_F(PaymentsNetworkInterfaceTest, NetworkError) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_REQUEST_TIMEOUT,
                 std::string());
  EXPECT_EQ(PaymentsRpcResult::kNetworkError, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsNetworkInterfaceTest, OtherError) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_FORBIDDEN,
                 std::string());
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsNetworkInterfaceTest, VcnRetrievalTryAgainFailure) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_temporary_error\" } }");
  EXPECT_EQ(PaymentsRpcResult::kVcnRetrievalTryAgainFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, VcnRetrievalPermanentFailure) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
  EXPECT_EQ(PaymentsRpcResult::kVcnRetrievalPermanentFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskPermanentFailureWhenVcnMissingExpiration) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\" }");

  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskPermanentFailureWhenVcnMissingCvv) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"expiration\": { "
                 "\"month\":12, \"year\":2099 } }");

  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskSuccessMeasureTimeoutHistogram) {
  base::HistogramTester histogram_tester;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillUnmaskCardRequestTimeout);

  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"pan\": \"1234\" }");

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsNetworkInterface.UnmaskCardRequest.ClientSideTimedOut",
      /*sample=*/false, /*expected_bucket_count=*/1);
}

TEST_F(PaymentsNetworkInterfaceTest, UnmaskFailureDueToClientSideTimeout) {
  base::HistogramTester histogram_tester;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillUnmaskCardRequestTimeout);

  // Fake a client-side timeout on the card unmask.
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::ERR_TIMED_OUT, "");

  EXPECT_EQ(PaymentsRpcResult::kClientSideTimeout, result_);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentsNetworkInterface.UnmaskCardRequest.ClientSideTimedOut",
      /*sample=*/true, /*expected_bucket_count=*/1);
}

TEST_F(PaymentsNetworkInterfaceTest,
       UnmaskClientTimeoutNotRecordedForOtherFailure) {
  base::HistogramTester histogram_tester;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillUnmaskCardRequestTimeout);

  // Fake a network issue on the unmask; this shouldn't result in any record
  // being made for the client timeout histogram. In particular,
  // HTTP_REQUEST_TIMEOUT is treated differently than the client side timeout.
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_REQUEST_TIMEOUT,
                 "");

  EXPECT_EQ(PaymentsRpcResult::kNetworkError, result_);
  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentsNetworkInterface.UnmaskCardRequest.ClientSideTimedOut",
      /*expected_count=*/0);
}

// Tests for the local card migration flow. Desktop only.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PaymentsNetworkInterfaceTest, GetDetailsFollowedByMigrationSuccess) {
  StartGettingUploadDetails();
  IssueOAuthToken();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);

  result_ = PaymentsRpcResult::kNone;

  StartMigrating();
  ReturnResponse(
      payments_network_interface_.get(), net::HTTP_OK,
      "{\"save_result\":[],\"value_prop_display_text\":\"display text\"}");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
}
#endif

// Tests for the local card migration flow. Desktop only.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PaymentsNetworkInterfaceTest, MigrateCardsVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartMigrating();
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());
}

TEST_F(PaymentsNetworkInterfaceTest, MigrationSuccessWithSaveResult) {
  StartMigrating();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{\"save_result\":[{\"unique_id\":\"0\",\"status\":"
                 "\"SUCCESS\"},{\"unique_id\":\"1\",\"status\":\"TEMPORARY_"
                 "FAILURE\"}],\"value_prop_display_text\":\"display text\"}");

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_TRUE(migration_save_results_.get());
  EXPECT_TRUE(migration_save_results_->find("0") !=
              migration_save_results_->end());
  EXPECT_TRUE(migration_save_results_->at("0") == "SUCCESS");
  EXPECT_TRUE(migration_save_results_->find("1") !=
              migration_save_results_->end());
  EXPECT_TRUE(migration_save_results_->at("1") == "TEMPORARY_FAILURE");
}

TEST_F(PaymentsNetworkInterfaceTest, MigrationMissingSaveResult) {
  StartMigrating();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{\"value_prop_display_text\":\"display text\"}");
  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ(nullptr, migration_save_results_.get());
}

TEST_F(PaymentsNetworkInterfaceTest, MigrationSuccessWithDisplayText) {
  StartMigrating();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{\"save_result\":[{\"unique_id\":\"0\",\"status\":"
                 "\"SUCCESS\"}],\"value_prop_display_text\":\"display text\"}");
  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("display text", display_text_);
}
#endif

TEST_F(PaymentsNetworkInterfaceTest, SelectChallengeOptionWithSmsOtpMethod) {
  StartSelectingChallengeOption(CardUnmaskChallengeOptionType::kSmsOtp,
                                "arbitrary id for sms otp");
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"context_token\": \"new context token\" }");

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  assertIncludedInRequest("context_token");
  assertIncludedInRequest("external_customer_id");
  assertIncludedInRequest("selected_idv_challenge_option");
  assertIncludedInRequest("sms_otp_challenge_option");
  // We should only set the challenge id. No need to send the masked phone
  // number.
  assertIncludedInRequest("challenge_id");
  assertIncludedInRequest("arbitrary id for sms otp");
  assertNotIncludedInRequest("masked_phone_number");
}

TEST_F(PaymentsNetworkInterfaceTest, SelectChallengeOptionSuccess) {
  StartSelectingChallengeOption();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"context_token\": \"new context token\" }");

  EXPECT_EQ(PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("new context token", context_token_);
}

TEST_F(PaymentsNetworkInterfaceTest, SelectChallengeOptionTemporaryFailure) {
  StartSelectingChallengeOption();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_temporary_error\"} }");

  EXPECT_EQ(PaymentsRpcResult::kVcnRetrievalTryAgainFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, SelectChallengeOptionVcnFlowPermanentFailure) {
  StartSelectingChallengeOption();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_permanent_error\"} }");

  EXPECT_EQ(PaymentsRpcResult::kVcnRetrievalPermanentFailure, result_);
}

TEST_F(PaymentsNetworkInterfaceTest, SelectChallengeOptionResponseMissingContextToken) {
  StartSelectingChallengeOption();
  IssueOAuthToken();
  ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");

  EXPECT_EQ(PaymentsRpcResult::kPermanentFailure, result_);
}

typedef std::tuple<VirtualCardEnrollmentSource,
                   VirtualCardEnrollmentRequestType,
                   PaymentsRpcResult>
    UpdateVirtualCardEnrollmentTestData;

class UpdateVirtualCardEnrollmentTest
    : public PaymentsNetworkInterfaceTest,
      public ::testing::WithParamInterface<
          UpdateVirtualCardEnrollmentTestData> {
 public:
  UpdateVirtualCardEnrollmentTest() = default;
  ~UpdateVirtualCardEnrollmentTest() override = default;

  void TriggerFlow() {
    VirtualCardEnrollmentSource virtual_card_enrollment_source =
        std::get<0>(GetParam());
    VirtualCardEnrollmentRequestType virtual_card_enrollment_request_type =
        std::get<1>(GetParam());
    StartUpdateVirtualCardEnrollment(virtual_card_enrollment_source,
                                     virtual_card_enrollment_request_type);
    IssueOAuthToken();

    // |response_type_for_test| is the PaymentsRpcResult
    // response type we want to test for the combination of
    // |virtual_card_enrollment_source| and
    // |virtual_card_enrollment_request_type| we are currently on.
    PaymentsRpcResult response_type_for_test = std::get<2>(GetParam());
    switch (response_type_for_test) {
      case PaymentsRpcResult::kSuccess:
        if (virtual_card_enrollment_request_type ==
            VirtualCardEnrollmentRequestType::kEnroll) {
          ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                         "{ \"enroll_result\": \"ENROLL_SUCCESS\" }");
        } else if (virtual_card_enrollment_request_type ==
                   VirtualCardEnrollmentRequestType::kUnenroll) {
          ReturnResponse(payments_network_interface_.get(), net::HTTP_OK, "{}");
        }
        break;
      case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
        ReturnResponse(
            payments_network_interface_.get(), net::HTTP_OK,
            "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
            "\"api_error_reason\": \"virtual_card_temporary_error\"} }");
        break;
      case PaymentsRpcResult::kTryAgainFailure:
        ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                       "{ \"error\": { \"code\": \"INTERNAL\", "
                       "\"api_error_reason\": \"ANYTHING_ELSE\"} }");
        break;
      case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
        ReturnResponse(
            payments_network_interface_.get(), net::HTTP_OK,
            "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
            "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
        break;
      case PaymentsRpcResult::kPermanentFailure:
        ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                       "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
        break;
      case PaymentsRpcResult::kNetworkError:
        ReturnResponse(payments_network_interface_.get(),
                       net::HTTP_REQUEST_TIMEOUT, "");
        break;
      case PaymentsRpcResult::kClientSideTimeout:
        ReturnResponse(payments_network_interface_.get(), net::ERR_TIMED_OUT,
                       "");
        break;
      case PaymentsRpcResult::kNone:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    EXPECT_EQ(response_type_for_test, result_);
  }

 private:
  void StartUpdateVirtualCardEnrollment(
      VirtualCardEnrollmentSource virtual_card_enrollment_source,
      VirtualCardEnrollmentRequestType virtual_card_enrollment_request_type) {
    PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails request_details;
    request_details.virtual_card_enrollment_request_type =
        virtual_card_enrollment_request_type;
    request_details.virtual_card_enrollment_source =
        virtual_card_enrollment_source;
    request_details.billing_customer_number = 555666777888;
    if (virtual_card_enrollment_request_type ==
        VirtualCardEnrollmentRequestType::kEnroll) {
      request_details.vcn_context_token = "fake context token";
    }
    request_details.instrument_id = 12345678;
    payments_network_interface_->UpdateVirtualCardEnrollment(
        request_details,
        base::BindOnce(
            &PaymentsNetworkInterfaceTest::OnDidGetUpdateVirtualCardEnrollmentResponse,
            GetWeakPtr()));
  }
};

// Initializes the parameterized test suite with all possible values of
// VirtualCardEnrollmentSource, VirtualCardEnrollmentRequestType, and
// PaymentsRpcResult.
INSTANTIATE_TEST_SUITE_P(
    ,
    UpdateVirtualCardEnrollmentTest,
    testing::Combine(
        testing::Values(VirtualCardEnrollmentSource::kUpstream,
                        VirtualCardEnrollmentSource::kDownstream,
                        VirtualCardEnrollmentSource::kSettingsPage),
        testing::Values(VirtualCardEnrollmentRequestType::kEnroll,
                        VirtualCardEnrollmentRequestType::kUnenroll),
        testing::Values(PaymentsRpcResult::kSuccess,
                        PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
                        PaymentsRpcResult::kTryAgainFailure,
                        PaymentsRpcResult::kVcnRetrievalPermanentFailure,
                        PaymentsRpcResult::kPermanentFailure,
                        PaymentsRpcResult::kNetworkError,
                        PaymentsRpcResult::kClientSideTimeout)));

// Parameterized test that tests all combinations of
// VirtualCardEnrollmentSource and VirtualCardEnrollmentRequestType against all
// possible server responses in the UpdateVirtualCardEnrollmentFlow. This test
// will be run once for each combination.
TEST_P(UpdateVirtualCardEnrollmentTest,
       UpdateVirtualCardEnrollmentTest_TestAllFlows) {
  TriggerFlow();
}

class GetVirtualCardEnrollmentDetailsTest
    : public PaymentsNetworkInterfaceTest,
      public ::testing::WithParamInterface<
          std::tuple<VirtualCardEnrollmentSource, PaymentsRpcResult>> {
 public:
  GetVirtualCardEnrollmentDetailsTest() = default;
  ~GetVirtualCardEnrollmentDetailsTest() override = default;
};

// Initializes the parameterized test suite with all possible combinations of
// VirtualCardEnrollmentSource and PaymentsRpcResult.
INSTANTIATE_TEST_SUITE_P(
    ,
    GetVirtualCardEnrollmentDetailsTest,
    testing::Combine(
        testing::Values(VirtualCardEnrollmentSource::kUpstream,
                        VirtualCardEnrollmentSource::kDownstream,
                        VirtualCardEnrollmentSource::kSettingsPage),
        testing::Values(PaymentsRpcResult::kSuccess,
                        PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
                        PaymentsRpcResult::kTryAgainFailure,
                        PaymentsRpcResult::kVcnRetrievalPermanentFailure,
                        PaymentsRpcResult::kPermanentFailure,
                        PaymentsRpcResult::kNetworkError,
                        PaymentsRpcResult::kClientSideTimeout)));

// Parameterized test that tests all combinations of
// VirtualCardEnrollmentSource and server PaymentsRpcResult. This test
// will be run once for each combination.
TEST_P(GetVirtualCardEnrollmentDetailsTest,
       GetVirtualCardEnrollmentDetailsTest_TestAllFlows) {
  VirtualCardEnrollmentSource source = std::get<0>(GetParam());

  PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails request_details;
  request_details.source = source;
  request_details.instrument_id = 12345678;
  request_details.billing_customer_number = 555666777888;
  request_details.risk_data = "fake risk data";
  request_details.app_locale = "en";

  payments_network_interface_->GetVirtualCardEnrollmentDetails(
      request_details,
      base::BindOnce(&PaymentsNetworkInterfaceTest::OnDidGetVirtualCardEnrollmentDetails,
                     GetWeakPtr()));
  IssueOAuthToken();

  // Ensures the PaymentsRpcResult is set correctly.
  PaymentsRpcResult result = std::get<1>(GetParam());
  switch (result) {
    case PaymentsRpcResult::kSuccess:
      ReturnResponse(
          payments_network_interface_.get(), net::HTTP_OK,
          "{ \"google_legal_message\": { \"line\" : [{ \"template\": \"This is "
          "the entire message.\" }] }, \"external_legal_message\": {}, "
          "\"context_token\": \"some_token\" }");
      break;
    case PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      ReturnResponse(
          payments_network_interface_.get(), net::HTTP_OK,
          "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
          "\"api_error_reason\": \"virtual_card_temporary_error\"} }");
      break;
    case PaymentsRpcResult::kTryAgainFailure:
      ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                     "{ \"error\": { \"code\": \"INTERNAL\", "
                     "\"api_error_reason\": \"ANYTHING_ELSE\"} }");
      break;
    case PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      ReturnResponse(
          payments_network_interface_.get(), net::HTTP_OK,
          "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
          "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
      break;
    case PaymentsRpcResult::kPermanentFailure:
      ReturnResponse(payments_network_interface_.get(), net::HTTP_OK,
                     "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
      break;
    case PaymentsRpcResult::kNetworkError:
      ReturnResponse(payments_network_interface_.get(),
                     net::HTTP_REQUEST_TIMEOUT, "");
      break;
    case PaymentsRpcResult::kClientSideTimeout:
      ReturnResponse(payments_network_interface_.get(), net::ERR_TIMED_OUT, "");
      break;
    case PaymentsRpcResult::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  EXPECT_EQ(result, result_);
}

}  // namespace
}  // namespace autofill::payments
