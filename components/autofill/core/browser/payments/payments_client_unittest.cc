// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_client.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::HasSubstr;

namespace autofill::payments {
namespace {

int kAllDetectableValues =
    CreditCardSaveManager::DetectedValue::CVC |
    CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME |
    CreditCardSaveManager::DetectedValue::ADDRESS_NAME |
    CreditCardSaveManager::DetectedValue::ADDRESS_LINE |
    CreditCardSaveManager::DetectedValue::LOCALITY |
    CreditCardSaveManager::DetectedValue::ADMINISTRATIVE_AREA |
    CreditCardSaveManager::DetectedValue::POSTAL_CODE |
    CreditCardSaveManager::DetectedValue::COUNTRY_CODE |
    CreditCardSaveManager::DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT;

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

struct GetUploadDetailsOptions {
  GetUploadDetailsOptions& with_upload_card_source(
      PaymentsClient::UploadCardSource u) {
    upload_card_source = u;
    return *this;
  }

  GetUploadDetailsOptions& with_billing_customer_number(int64_t i) {
    billing_customer_number = i;
    return *this;
  }

  GetUploadDetailsOptions& with_client_behavior_signals(
      std::vector<ClientBehaviorConstants> v) {
    client_behavior_signals = std::move(v);
    return *this;
  }

  PaymentsClient::UploadCardSource upload_card_source =
      PaymentsClient::UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE;
  int64_t billing_customer_number = 111222333444L;
  std::vector<ClientBehaviorConstants> client_behavior_signals;
};

struct UploadCardOptions {
  UploadCardOptions& with_cvc_in_request(bool b) {
    include_cvc = b;
    return *this;
  }

  UploadCardOptions& with_nickname_in_request(bool b) {
    include_nickname = b;
    return *this;
  }

  UploadCardOptions& with_billing_customer_number(int64_t i) {
    billing_customer_number = i;
    return *this;
  }

  UploadCardOptions& with_client_behavior_signals(
      std::vector<ClientBehaviorConstants> v) {
    client_behavior_signals = std::move(v);
    return *this;
  }

  bool include_cvc = false;
  bool include_nickname = false;
  int64_t billing_customer_number = 111222333444L;
  std::vector<ClientBehaviorConstants> client_behavior_signals;
};

}  // namespace

class PaymentsClientTest : public testing::Test {
 public:
  PaymentsClientTest() = default;

  PaymentsClientTest(const PaymentsClientTest&) = delete;
  PaymentsClientTest& operator=(const PaymentsClientTest&) = delete;

  ~PaymentsClientTest() override = default;

  void SetUp() override {
    // Silence the warning for mismatching sync and Payments servers.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWalletServiceUseSandbox, "0");

    result_ = AutofillClient::PaymentsRpcResult::kNone;
    legal_message_.reset();
    has_variations_header_ = false;

    factory()->SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          intercepted_headers_ = request.headers;
          intercepted_body_ = network::GetUploadData(request);
          has_variations_header_ = variations::HasVariationsHeader(request);
        }));
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    client_ = std::make_unique<PaymentsClient>(
        test_shared_loader_factory_, identity_test_env_.identity_manager(),
        &test_personal_data_);
    test_personal_data_.SetAccountInfoForPayments(
        identity_test_env_.MakePrimaryAccountAvailable(
            "example@gmail.com", signin::ConsentLevel::kSync));
  }

  void TearDown() override { client_.reset(); }

  // Registers a field trial with the specified name and group and an associated
  // google web property variation id.
  void CreateFieldTrialWithId(const std::string& trial_name,
                              const std::string& group_name,
                              int variation_id) {
    variations::AssociateGoogleVariationID(
        variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, trial_name, group_name,
        static_cast<variations::VariationID>(variation_id));
    base::FieldTrialList::CreateFieldTrial(trial_name, group_name)->Activate();
  }

  void OnDidGetUnmaskDetails(
      AutofillClient::PaymentsRpcResult result,
      payments::PaymentsClient::UnmaskDetails& unmask_details) {
    result_ = result;
    unmask_details_ = unmask_details;
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       PaymentsClient::UnmaskResponseDetails& response) {
    result_ = result;
    unmask_response_details_ = response;
  }

  void OnDidGetOptChangeResult(
      AutofillClient::PaymentsRpcResult result,
      PaymentsClient::OptChangeResponseDetails& response) {
    result_ = result;
    opt_change_response_.user_is_opted_in = response.user_is_opted_in;
    opt_change_response_.fido_creation_options =
        std::move(response.fido_creation_options);
    opt_change_response_.fido_request_options =
        std::move(response.fido_request_options);
  }

  void OnDidGetUploadDetails(
      AutofillClient::PaymentsRpcResult result,
      const std::u16string& context_token,
      std::unique_ptr<base::Value::Dict> legal_message,
      std::vector<std::pair<int, int>> supported_card_bin_ranges) {
    result_ = result;
    legal_message_ = std::move(legal_message);
    supported_card_bin_ranges_ = supported_card_bin_ranges;
  }

  void OnDidUploadCard(AutofillClient::PaymentsRpcResult result,
                       const PaymentsClient::UploadCardResponseDetails&
                           upload_card_respone_details) {
    result_ = result;
    upload_card_response_details_ = upload_card_respone_details;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void OnDidMigrateLocalCards(
      AutofillClient::PaymentsRpcResult result,
      std::unique_ptr<std::unordered_map<std::string, std::string>>
          migration_save_results,
      const std::string& display_text) {
    result_ = result;
    migration_save_results_ = std::move(migration_save_results);
    display_text_ = display_text;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  void OnDidSelectChallengeOption(AutofillClient::PaymentsRpcResult result,
                                  const std::string& updated_context_token) {
    result_ = result;
    context_token_ = updated_context_token;
  }

  void OnDidGetVirtualCardEnrollmentDetails(
      AutofillClient::PaymentsRpcResult result,
      const payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails&
          get_details_for_enrollment_response_fields) {
    result_ = result;
    get_details_for_enrollment_response_fields_ =
        get_details_for_enrollment_response_fields;
  }

  void OnDidGetUpdateVirtualCardEnrollmentResponse(
      AutofillClient::PaymentsRpcResult result) {
    result_ = result;
  }

 protected:
  // Issue a GetUnmaskDetails request. This requires an OAuth token before
  // starting the request.
  void StartGettingUnmaskDetails() {
    client_->GetUnmaskDetails(
        base::BindOnce(&PaymentsClientTest::OnDidGetUnmaskDetails,
                       GetWeakPtr()),
        "language-LOCALE");
  }

  // Issue an UnmaskCard request. This requires an OAuth token before starting
  // the request.
  void StartUnmasking(CardUnmaskOptions options) {
    PaymentsClient::UnmaskRequestDetails request_details;
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
    client_->UnmaskCard(
        request_details,
        base::BindOnce(&PaymentsClientTest::OnDidGetRealPan, GetWeakPtr()));
  }

  // If |opt_in| is set to true, then opts the user in to use FIDO
  // authentication for card unmasking. Otherwise opts the user out.
  void StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::Reason reason) {
    PaymentsClient::OptChangeRequestDetails request_details;
    request_details.reason = reason;
    client_->OptChange(
        request_details,
        base::BindOnce(&PaymentsClientTest::OnDidGetOptChangeResult,
                       GetWeakPtr()));
  }

  // Issue a GetUploadDetails request. This may require an OAuth token before
  // starting the request.
  void StartGettingUploadDetails(
      GetUploadDetailsOptions get_upload_details_options) {
    client_->GetUploadDetails(
        BuildTestProfiles(), kAllDetectableValues,
        get_upload_details_options.client_behavior_signals, "language-LOCALE",
        base::BindOnce(&PaymentsClientTest::OnDidGetUploadDetails,
                       GetWeakPtr()),
        /*billable_service_number=*/12345,
        get_upload_details_options.billing_customer_number,
        get_upload_details_options.upload_card_source);
  }

  // Issue an UploadCard request. This requires an OAuth token before starting
  // the request.
  void StartUploading(UploadCardOptions upload_card_options) {
    PaymentsClient::UploadRequestDetails request_details;
    request_details.billing_customer_number =
        upload_card_options.billing_customer_number;
    request_details.card = test::GetCreditCard();
    if (upload_card_options.include_cvc) {
      request_details.cvc = u"123";
    }
    if (upload_card_options.include_nickname) {
      upstream_nickname_ = u"grocery";
      request_details.card.SetNickname(upstream_nickname_);
    }
    request_details.client_behavior_signals =
        upload_card_options.client_behavior_signals;

    request_details.context_token = u"context token";
    request_details.risk_data = "some risk data";
    request_details.app_locale = "language-LOCALE";
    request_details.profiles = BuildTestProfiles();
    client_->UploadCard(
        request_details,
        base::BindOnce(&PaymentsClientTest::OnDidUploadCard, GetWeakPtr()));
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void StartMigrating(bool has_cardholder_name,
                      bool set_nickname_for_first_card = false) {
    PaymentsClient::MigrationRequestDetails request_details;
    request_details.context_token = u"context token";
    request_details.risk_data = "some risk data";
    request_details.app_locale = "language-LOCALE";

    migratable_credit_cards_.clear();
    CreditCard card1 = test::GetCreditCard();
    if (set_nickname_for_first_card)
      card1.SetNickname(u"grocery");
    CreditCard card2 = test::GetCreditCard2();
    if (!has_cardholder_name) {
      card1.SetRawInfo(CREDIT_CARD_NAME_FULL, u"");
      card2.SetRawInfo(CREDIT_CARD_NAME_FULL, u"");
    }
    migratable_credit_cards_.emplace_back(card1);
    migratable_credit_cards_.emplace_back(card2);
    client_->MigrateCards(
        request_details, migratable_credit_cards_,
        base::BindOnce(&PaymentsClientTest::OnDidMigrateLocalCards,
                       GetWeakPtr()));
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  void StartSelectingChallengeOption(
      CardUnmaskChallengeOptionType challenge_type =
          CardUnmaskChallengeOptionType::kSmsOtp,
      std::string challenge_id = "arbitrary id") {
    PaymentsClient::SelectChallengeOptionRequestDetails request_details;
    request_details.billing_customer_number = 555666777888;
    request_details.context_token = "fake context token";

    CardUnmaskChallengeOption selected_challenge_option;
    selected_challenge_option.type = challenge_type;
    selected_challenge_option.id =
        CardUnmaskChallengeOption::ChallengeOptionId(challenge_id);
    selected_challenge_option.challenge_info = u"(***)-***-5678";
    request_details.selected_challenge_option = selected_challenge_option;

    client_->SelectChallengeOption(
        request_details,
        base::BindOnce(&PaymentsClientTest::OnDidSelectChallengeOption,
                       GetWeakPtr()));
  }

  network::TestURLLoaderFactory* factory() { return &test_url_loader_factory_; }

  const std::string& GetUploadData() { return intercepted_body_; }

  bool HasVariationsHeader() { return has_variations_header_; }

  // Issues access token in response to any access token request. This will
  // start the Payments Request which requires the authentication.
  void IssueOAuthToken() {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        "totally_real_token", AutofillClock::Now() + base::Days(10));

    // Verify the auth header.
    std::string auth_header_value;
    EXPECT_TRUE(intercepted_headers_.GetHeader(
        net::HttpRequestHeaders::kAuthorization, &auth_header_value))
        << intercepted_headers_.ToString();
    EXPECT_EQ("Bearer totally_real_token", auth_header_value);
  }

  void ReturnResponse(net::HttpStatusCode response_code,
                      const std::string& response_body) {
    client_->OnSimpleLoaderCompleteInternal(response_code, response_body);
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

  const PaymentsClient::UnmaskDetails* unmask_details() const {
    return unmask_details_ ? &unmask_details_.value() : nullptr;
  }
  const PaymentsClient::UnmaskResponseDetails* unmask_response_details() const {
    return unmask_response_details_ ? &unmask_response_details_.value()
                                    : nullptr;
  }
  void ResetUnmaskResponseDetails() { unmask_response_details_.reset(); }

  base::WeakPtr<PaymentsClientTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  AutofillClient::PaymentsRpcResult result_ =
      AutofillClient::PaymentsRpcResult::kNone;

  // Server ID of a saved card via credit card upload save.
  PaymentsClient::UploadCardResponseDetails upload_card_response_details_;
  // The OptChangeResponseDetails retrieved from an OptChangeRequest.
  PaymentsClient::OptChangeResponseDetails opt_change_response_;
  // The response details retrieved from an GetDetailsForEnrollmentRequest.
  PaymentsClient::GetDetailsForEnrollmentResponseDetails
      get_details_for_enrollment_response_fields_;
  // The legal message returned from a GetDetails upload save preflight call.
  std::unique_ptr<base::Value::Dict> legal_message_;
  // A list of card BIN ranges supported by Google Payments, returned from a
  // GetDetails upload save preflight call.
  std::vector<std::pair<int, int>> supported_card_bin_ranges_;
  // The nickname name in the UploadRequest that was supposed to be saved.
  std::u16string upstream_nickname_;
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

  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  TestPersonalDataManager test_personal_data_;
  std::unique_ptr<PaymentsClient> client_;
  signin::IdentityTestEnvironment identity_test_env_;

  net::HttpRequestHeaders intercepted_headers_;
  bool has_variations_header_;
  std::string intercepted_body_;
 private:
  std::vector<AutofillProfile> BuildTestProfiles() {
    std::vector<AutofillProfile> profiles;
    profiles.push_back(BuildProfile("John", "Smith", "1234 Main St.", "Miami",
                                    "FL", "32006", "212-555-0162"));
    profiles.push_back(BuildProfile("Pat", "Jones", "432 Oak Lane", "Lincoln",
                                    "OH", "43005", "(834)555-0090"));
    return profiles;
  }

  AutofillProfile BuildProfile(base::StringPiece first_name,
                               base::StringPiece last_name,
                               base::StringPiece address_line,
                               base::StringPiece city,
                               base::StringPiece state,
                               base::StringPiece zip,
                               base::StringPiece phone_number) {
    AutofillProfile profile;

    profile.SetInfo(NAME_FIRST, base::ASCIIToUTF16(first_name), "en-US");
    profile.SetInfo(NAME_LAST, base::ASCIIToUTF16(last_name), "en-US");
    profile.SetInfo(ADDRESS_HOME_LINE1, base::ASCIIToUTF16(address_line),
                    "en-US");
    profile.SetInfo(ADDRESS_HOME_CITY, base::ASCIIToUTF16(city), "en-US");
    profile.SetInfo(ADDRESS_HOME_STATE, base::ASCIIToUTF16(state), "en-US");
    profile.SetInfo(ADDRESS_HOME_ZIP, base::ASCIIToUTF16(zip), "en-US");
    profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, base::ASCIIToUTF16(phone_number),
                    "en-US");
    profile.FinalizeAfterImport();
    return profile;
  }

  absl::optional<PaymentsClient::UnmaskDetails> unmask_details_;
  // The UnmaskResponseDetails retrieved from an UnmaskRequest.  Includes PAN.
  absl::optional<PaymentsClient::UnmaskResponseDetails>
      unmask_response_details_;
  base::WeakPtrFactory<PaymentsClientTest> weak_ptr_factory_{this};
};

TEST_F(PaymentsClientTest, GetUnmaskDetailsSuccess) {
  StartGettingUnmaskDetails();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"offer_fido_opt_in\": \"false\", "
                 "\"authentication_method\": \"CVC\" }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ(false, unmask_details()->offer_fido_opt_in);
  EXPECT_EQ(AutofillClient::UnmaskAuthMethod::kCvc,
            unmask_details()->unmask_auth_method);
}

TEST_F(PaymentsClientTest, GetUnmaskDetailsIncludesChromeUserContext) {
  StartGettingUnmaskDetails();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest, OAuthError) {
  StartUnmasking(CardUnmaskOptions());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_TRUE(unmask_response_details()->real_pan.empty());
}

TEST_F(PaymentsClientTest,
       UnmaskRequestIncludesBillingCustomerNumberInRequest) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();

  // Verify that the billing customer number is included in the request.
  EXPECT_TRUE(
      GetUploadData().find("%22external_customer_id%22:%22111222333444%22") !=
      std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskSuccessViaCVC) {
  StartUnmasking(CardUnmaskOptions().with_cvc("111"));
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");

  assertCvcIncludedInRequest("111");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("1234", unmask_response_details()->real_pan);
}

TEST_F(PaymentsClientTest, UnmaskSuccessViaFIDO) {
  StartUnmasking(CardUnmaskOptions().with_fido());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");

  assertCvcNotIncludedInRequest();
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("1234", unmask_response_details()->real_pan);
}

TEST_F(PaymentsClientTest, UnmaskSuccessViaCVCWithCreationOptions) {
  StartUnmasking(CardUnmaskOptions().with_cvc("111"));
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\", \"dcvv\": \"321\"}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("1234", unmask_response_details()->real_pan);
  EXPECT_EQ("321", unmask_response_details()->dcvv);
}

TEST_F(PaymentsClientTest, UnmaskSuccessAccountFromSyncTest) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("1234", unmask_response_details()->real_pan);
}

TEST_F(PaymentsClientTest, UnmaskSuccessWithVirtualCardCvcAuth) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card().with_cvc("222"));
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\",  "
                 "\"expiration\": { \"month\":12, \"year\":2099 } }");

  assertCvcIncludedInRequest("222");
  assertIncludedInRequest("cvc_challenge_option");
  assertIncludedInRequest("challenge_id");
  assertIncludedInRequest("cvc_length");
  assertIncludedInRequest("cvc_position");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
}

TEST_F(PaymentsClientTest, UnmaskSuccessWithVirtualCardFidoAuth) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card().with_fido());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\",  "
                 "\"expiration\": { \"month\":12, \"year\":2099 } }");

  assertCvcNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
}

TEST_F(PaymentsClientTest, VirtualCardRiskBasedGreenPathResponse) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\",  "
                 "\"expiration\": { \"month\":12, \"year\":2099 } }");

  // Verify that Cvc/FIDO/OTP are not included in the request.
  assertCvcNotIncludedInRequest();
  assertOtpNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  EXPECT_TRUE(GetUploadData().find("fido_assertion_info") == std::string::npos);
  // Only merchant_domain is included.
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
  EXPECT_TRUE(unmask_response_details()->card_unmask_challenge_options.empty());
}

TEST_F(PaymentsClientTest, VirtualCardRiskBasedRedPathResponse_Error) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"NON-INTERNAL\", "
                 "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure,
            result_);
}

TEST_F(PaymentsClientTest,
       VirtualCardRiskBasedRedPathResponse_NoOptionProvided) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"context_token\": \"fake_context_token\" }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
}

TEST_F(PaymentsClientTest, VirtualCardRiskBasedYellowPathResponse) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
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
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("fake_context_token", unmask_response_details()->context_token);
  // Verify the FIDO request challenge is correctly parsed.
  EXPECT_EQ("fake_fido_challenge",
            *unmask_response_details()->fido_request_options->FindString(
                "challenge"));
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

TEST_F(PaymentsClientTest, VirtualCardCvcRetrieval_FlowStatusPresent) {
  StartUnmasking(
      CardUnmaskOptions().with_virtual_card_risk_based().with_cvc("123"));
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{\"flow_status\": \"FLOW_STATUS_INCORRECT_ACCOUNT_SECURITY_CODE\"}");

  // Ensure that it is treated as a try again failure when a flow status is
  // returned.
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kTryAgainFailure, result_);
}

TEST_F(PaymentsClientTest,
       VirtualCardRiskBasedYellowPathResponseWithUnknownType) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based());
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"fido_request_options\": { \"challenge\": \"fake_fido_challenge\" }, "
      "\"context_token\": \"fake_context_token\", \"idv_challenge_options\": "
      "[{ \"sms_otp_challenge_option\": { \"challenge_id\": "
      "\"fake_challenge_id_1\", \"masked_phone_number\": \"(***)-***-1234\" } "
      "}, { \"unknown_new_challenge_option\": { \"challenge_id\": "
      "\"fake_challenge_id_2\" } }] }");

  // Ensure that it's not treated as failure when no pan is returned.
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("fake_context_token", unmask_response_details()->context_token);
  // Verify the FIDO request challenge is correctly parsed.
  EXPECT_EQ("fake_fido_challenge",
            *unmask_response_details()->fido_request_options->FindString(
                "challenge"));
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

TEST_F(PaymentsClientTest, VirtualCardRiskBasedThenFido) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card_risk_based_then_fido());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
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

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
}

TEST_F(PaymentsClientTest, VirtualCardRiskBasedThenOtpSuccess) {
  const std::string otp = "111111";
  StartUnmasking(
      CardUnmaskOptions().with_virtual_card_risk_based_then_otp(otp));
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
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

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ("999", unmask_response_details()->dcvv);
  EXPECT_EQ("12", unmask_response_details()->expiration_month);
  EXPECT_EQ("2099", unmask_response_details()->expiration_year);
}

TEST_F(PaymentsClientTest, ExpiredOtp) {
  const std::string otp = "222222";
  StartUnmasking(
      CardUnmaskOptions().with_virtual_card_risk_based_then_otp(otp));
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"context_token\": \"fake_context_token\", "
                 "\"flow_status\": \"FLOW_STATUS_EXPIRED_OTP\" }");

  assertOtpIncludedInRequest(otp);
  assertCvcNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  // Verify the context token is also included.
  EXPECT_TRUE(GetUploadData().find("context_token") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("FLOW_STATUS_EXPIRED_OTP", unmask_response_details()->flow_status);
}

TEST_F(PaymentsClientTest, IncorrectOtp) {
  const std::string otp = "333333";
  StartUnmasking(
      CardUnmaskOptions().with_virtual_card_risk_based_then_otp(otp));
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"context_token\": \"fake_context_token\", "
                 "\"flow_status\": \"FLOW_STATUS_INCORRECT_OTP\" }");

  assertOtpIncludedInRequest(otp);
  assertCvcNotIncludedInRequest();
  assertNotIncludedInRequest("cvc_challenge_option");
  // Verify the context token is also included.
  EXPECT_TRUE(GetUploadData().find("context_token") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("FLOW_STATUS_INCORRECT_OTP",
            unmask_response_details()->flow_status);
}

TEST_F(PaymentsClientTest, UnmaskIncludesLegacyAndNonLegacyId) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // Non-legacy Instrument id and legacy server id are both set.
  EXPECT_TRUE(GetUploadData().find("%22instrument_id%22:%221%22") !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("%22credit_card_id%22:%22a123%22") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskIncludesOnlyLegacyId) {
  StartUnmasking(CardUnmaskOptions().with_only_legacy_id());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // Only legacy server id is set.
  EXPECT_TRUE(GetUploadData().find("instrument_id") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("%22credit_card_id%22:%22a123%22") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskIncludesOnlyNonLegacyId) {
  StartUnmasking(CardUnmaskOptions().with_only_non_legacy_id());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // Only non-legacy instrument id is set.
  EXPECT_TRUE(GetUploadData().find("%22instrument_id%22:%221%22") !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("credit_card_id") == std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskIncludesChromeUserContext) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskIncludesMerchantDomain) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  // last_committed_primary_main_frame_origin was set.
  EXPECT_TRUE(GetUploadData().find("merchant_domain") != std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskResponseIncludesDeclineDetails) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{\"error\": {\"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_temporary_error\"}, "
                 "\"decline_details\": {\"user_message_title\": "
                 "\"test_user_message_title\", \"user_message_description\": "
                 "\"test_user_message_description\"}}");

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
            result_);
  EXPECT_TRUE(
      unmask_response_details()->autofill_error_dialog_context.has_value());
  AutofillErrorDialogContext autofill_error_dialog_context =
      *unmask_response_details()->autofill_error_dialog_context;
  EXPECT_EQ(*autofill_error_dialog_context.server_returned_title,
            "test_user_message_title");
  EXPECT_EQ(*autofill_error_dialog_context.server_returned_description,
            "test_user_message_description");
}

TEST_F(PaymentsClientTest, UnmaskResponseIncludesEmptyDeclineDetails) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{\"error\": {\"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_temporary_error\"}, "
                 "\"decline_details\": {\"user_message_title\": "
                 "\"\", \"user_message_description\": "
                 "\"\"}}");

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
            result_);
  EXPECT_FALSE(
      unmask_response_details()->autofill_error_dialog_context.has_value());
}

TEST_F(PaymentsClientTest, OptInSuccess) {
  StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::ENABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"fido_authentication_info\": { \"user_status\": "
                 "\"FIDO_AUTH_ENABLED\"}}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_TRUE(opt_change_response_.user_is_opted_in.value());
}

TEST_F(PaymentsClientTest, OptInServerUnresponsive) {
  StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::ENABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_REQUEST_TIMEOUT, "");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kNetworkError, result_);
  EXPECT_FALSE(opt_change_response_.user_is_opted_in.has_value());
}

TEST_F(PaymentsClientTest, OptOutSuccess) {
  StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::DISABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"fido_authentication_info\": { \"user_status\": "
                 "\"FIDO_AUTH_DISABLED\"}}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_FALSE(opt_change_response_.user_is_opted_in.value());
}

TEST_F(PaymentsClientTest, EnrollAttemptReturnsCreationOptions) {
  StartOptChangeRequest(
      PaymentsClient::OptChangeRequestDetails::ENABLE_FIDO_AUTH);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"fido_authentication_info\": { \"user_status\": "
                 "\"FIDO_AUTH_DISABLED\","
                 "\"fido_creation_options\": {"
                 "\"relying_party_id\": \"google.com\"}}}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_FALSE(opt_change_response_.user_is_opted_in.value());
  EXPECT_EQ("google.com",
            *opt_change_response_.fido_creation_options->FindString(
                "relying_party_id"));
}

TEST_F(PaymentsClientTest, GetDetailsSuccess) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_NE(nullptr, legal_message_.get());
}

TEST_F(PaymentsClientTest, GetDetailsRemovesNonLocationData) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();

  // Verify that the recipient name field and test names appear nowhere in the
  // upload data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kRecipientName) ==
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("John") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Smith") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Pat") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Jones") == std::string::npos);

  // Verify that the phone number field and test numbers appear nowhere in the
  // upload data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kPhoneNumber) ==
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("212") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("555") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0162") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("834") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0090") == std::string::npos);
}

TEST_F(PaymentsClientTest, GetDetailsIncludesDetectedValuesInRequest) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();

  // Verify that the detected values were included in the request.
  std::string detected_values_string =
      "\"detected_values\":" + base::NumberToString(kAllDetectableValues);
  EXPECT_TRUE(GetUploadData().find(detected_values_string) !=
              std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsIncludesIncludesClientBehaviorSignalsInChromeUserContext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableNewSaveCardBubbleUi);

  StartGettingUploadDetails(
      GetUploadDetailsOptions().with_client_behavior_signals(
          std::vector<ClientBehaviorConstants>{
              ClientBehaviorConstants::kUsingFasterAndProtectedUi}));
  IssueOAuthToken();

  // Verify ChromeUserContext was set.
  EXPECT_THAT(GetUploadData(), HasSubstr("chrome_user_context"));
  // Verify Client_behavior_signals was set.
  EXPECT_THAT(GetUploadData(), HasSubstr("client_behavior_signals"));
  // Verify fake_client_behavior_signal was set.
  // ClientBehaviorConstants::kUsingFasterAndProtectedUi has the numeric value
  // set to 1.
  EXPECT_THAT(GetUploadData(), HasSubstr("\"client_behavior_signals\":[1]"));
}

TEST_F(PaymentsClientTest, GetDetailsIncludesChromeUserContext) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsIncludesUpstreamCheckoutFlowUploadCardSourceInRequest) {
  StartGettingUploadDetails(GetUploadDetailsOptions().with_upload_card_source(
      PaymentsClient::UploadCardSource::UPSTREAM_CHECKOUT_FLOW));
  IssueOAuthToken();

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("UPSTREAM_CHECKOUT_FLOW") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsIncludesUpstreamSettingsPageUploadCardSourceInRequest) {
  StartGettingUploadDetails(GetUploadDetailsOptions().with_upload_card_source(
      PaymentsClient::UploadCardSource::UPSTREAM_SETTINGS_PAGE));
  IssueOAuthToken();

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("UPSTREAM_SETTINGS_PAGE") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsIncludesUpstreamCardOcrUploadCardSourceInRequest) {
  StartGettingUploadDetails(GetUploadDetailsOptions().with_upload_card_source(
      PaymentsClient::UploadCardSource::UPSTREAM_CARD_OCR));
  IssueOAuthToken();

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("UPSTREAM_CARD_OCR") != std::string::npos);
}

TEST_F(
    PaymentsClientTest,
    GetDetailsIncludesLocalCardMigrationCheckoutFlowUploadCardSourceInRequest) {
  StartGettingUploadDetails(GetUploadDetailsOptions().with_upload_card_source(
      PaymentsClient::UploadCardSource::LOCAL_CARD_MIGRATION_CHECKOUT_FLOW));
  IssueOAuthToken();

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("LOCAL_CARD_MIGRATION_CHECKOUT_FLOW") !=
              std::string::npos);
}

TEST_F(
    PaymentsClientTest,
    GetDetailsIncludesLocalCardMigrationSettingsPageUploadCardSourceInRequest) {
  StartGettingUploadDetails(GetUploadDetailsOptions().with_upload_card_source(
      PaymentsClient::UploadCardSource::LOCAL_CARD_MIGRATION_SETTINGS_PAGE));
  IssueOAuthToken();

  // Verify that the correct upload card source was included in the request.
  EXPECT_TRUE(GetUploadData().find("LOCAL_CARD_MIGRATION_SETTINGS_PAGE") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, GetDetailsIncludesUnknownUploadCardSourceInRequest) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();

  // Verify that the absence of an upload card source results in UNKNOWN.
  EXPECT_TRUE(GetUploadData().find("UNKNOWN_UPLOAD_CARD_SOURCE") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, GetUploadDetailsVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());
}

TEST_F(PaymentsClientTest, GetDetailsIncludeBillableServiceNumber) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();

  // Verify that billable service number was included in the request.
  EXPECT_TRUE(GetUploadData().find("\"billable_service\":12345") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, GetDetailsIncludeBillingCustomerNumber) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();

  // Verify that the billing customer number is included in the request if flag
  // is enabled.
  EXPECT_TRUE(
      GetUploadData().find("\"external_customer_id\":\"111222333444\"") !=
      std::string::npos);
}

TEST_F(PaymentsClientTest,
       GetDetailsExcludesBillingCustomerNumberIfNoBcnExists) {
  // A value of zero is treated as a non-existent BCN.
  StartGettingUploadDetails(
      GetUploadDetailsOptions().with_billing_customer_number(0L));
  IssueOAuthToken();

  // Verify that the billing customer number is not included in the request if
  // billing customer number is 0.
  EXPECT_TRUE(GetUploadData().find("\"external_customer_id\"") ==
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("\"customer_context\"") ==
              std::string::npos);
}

TEST_F(PaymentsClientTest, GetDetailsFollowedByUploadSuccess) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);

  result_ = AutofillClient::PaymentsRpcResult::kNone;

  StartUploading(UploadCardOptions());
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
}

TEST_F(PaymentsClientTest, GetDetailsMissingContextToken) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
}

TEST_F(PaymentsClientTest, GetDetailsMissingLegalMessage) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"context_token\": \"some_token\" }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ(nullptr, legal_message_.get());
}

TEST_F(PaymentsClientTest, SupportedCardBinRangesParsesCorrectly) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{"
      "  \"context_token\" : \"some_token\","
      "  \"legal_message\" : {},"
      "  \"supported_card_bin_ranges_string\" : \"1234,300000-555555,765\""
      "}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  // Check that |supported_card_bin_ranges_| has the two entries specified in
  // ReturnResponse(~) above.
  ASSERT_EQ(3U, supported_card_bin_ranges_.size());
  EXPECT_EQ(1234, supported_card_bin_ranges_[0].first);
  EXPECT_EQ(1234, supported_card_bin_ranges_[0].second);
  EXPECT_EQ(300000, supported_card_bin_ranges_[1].first);
  EXPECT_EQ(555555, supported_card_bin_ranges_[1].second);
  EXPECT_EQ(765, supported_card_bin_ranges_[2].first);
  EXPECT_EQ(765, supported_card_bin_ranges_[2].second);
}

TEST_F(PaymentsClientTest, GetUploadAccountFromSyncTest) {
  // Set up a different account.
  const AccountInfo& secondary_account_info =
      identity_test_env_.MakeAccountAvailable("secondary@gmail.com");
  test_personal_data_.SetAccountInfoForPayments(secondary_account_info);

  StartUploading(UploadCardOptions());
  ReturnResponse(net::HTTP_OK, "{}");

  // Issue a token for the secondary account.
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      secondary_account_info.account_id, "secondary_account_token",
      AutofillClock::Now() + base::Days(10));

  // Verify the auth header.
  std::string auth_header_value;
  EXPECT_TRUE(intercepted_headers_.GetHeader(
      net::HttpRequestHeaders::kAuthorization, &auth_header_value))
      << intercepted_headers_.ToString();
  EXPECT_EQ("Bearer secondary_account_token", auth_header_value);
}

TEST_F(PaymentsClientTest, UploadCardVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartUploading(UploadCardOptions());
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());
}

TEST_F(PaymentsClientTest, UnmaskCardVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());
}

TEST_F(PaymentsClientTest, UploadSuccessEmptyResponse) {
  StartUploading(UploadCardOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_FALSE(upload_card_response_details_.instrument_id.has_value());
  EXPECT_TRUE(upload_card_response_details_.virtual_card_enrollment_state ==
              CreditCard::VirtualCardEnrollmentState::kUnspecified);
  EXPECT_TRUE(upload_card_response_details_.card_art_url.is_empty());
}

TEST_F(PaymentsClientTest, UploadSuccessInstrumentIdPresent) {
  StartUploading(UploadCardOptions());
  IssueOAuthToken();
  upload_card_response_details_.instrument_id = absl::nullopt;

  // Test the conversion from string to int64_t using the max value for int64_t.
  ReturnResponse(net::HTTP_OK,
                 "{ \"instrument_id\": \"9223372036854775807\" }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ(upload_card_response_details_.instrument_id, 9223372036854775807);
}

TEST_F(PaymentsClientTest, UploadSuccessVirtualCardEnrollmentStatePresent) {
  bool oauth_token_issued = false;
  for (CreditCard::VirtualCardEnrollmentState virtual_card_enrollment_state :
       {CreditCard::VirtualCardEnrollmentState::kUnenrolledAndNotEligible,
        CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible,
        CreditCard::VirtualCardEnrollmentState::kEnrolled}) {
    StartUploading(UploadCardOptions());
    // An OAuthToken needs to be issued to initiate the first UploadCard call
    // from PaymentsClientTest::StartUploading(), but only for the first call.
    // All future calls will use the first OAuthToken. If multiple OAuthTokens
    // are issued this test will time out.
    if (!oauth_token_issued) {
      IssueOAuthToken();
      oauth_token_issued = true;
    }
    switch (virtual_card_enrollment_state) {
      case CreditCard::VirtualCardEnrollmentState::kUnenrolledAndNotEligible:
        ReturnResponse(net::HTTP_OK,
                       "{ \"virtual_card_metadata\": { \"status\": "
                       "\"ENROLLMENT_STATUS_UNSPECIFIED\" } }");
        break;
      case CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible:
        ReturnResponse(net::HTTP_OK,
                       "{ \"virtual_card_metadata\": { \"status\": "
                       "\"ENROLLMENT_ELIGIBLE\" } }");
        break;
      case CreditCard::VirtualCardEnrollmentState::kEnrolled:
        ReturnResponse(
            net::HTTP_OK,
            "{ \"virtual_card_metadata\": { \"status\": \"ENROLLED\" } }");
        break;
      case CreditCard::VirtualCardEnrollmentState::kUnenrolled:
      case CreditCard::VirtualCardEnrollmentState::kUnspecified:
        break;
    }
    EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
    EXPECT_EQ(upload_card_response_details_.virtual_card_enrollment_state,
              virtual_card_enrollment_state);
  }
}

TEST_F(PaymentsClientTest,
       UploadSuccessGetDetailsForEnrollmentResponseDetailsPresent) {
  StartUploading(UploadCardOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"virtual_card_metadata\": "
                 "{\"status\": \"ENROLLMENT_ELIGIBLE\", "
                 "\"virtual_card_enrollment_data\": { "
                 "\"google_legal_message\": { \"line\" : [{ "
                 "\"template\": \"This is the entire message.\" }] }, "
                 "\"external_legal_message\": {},"
                 "\"context_token\": \"some_token\"} } }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
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

TEST_F(PaymentsClientTest, UploadSuccessCardArtUrlPresent) {
  StartUploading(UploadCardOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"card_art_url\": \"https://www.example.com/\" }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ(upload_card_response_details_.card_art_url.spec(),
            "https://www.example.com/");
}

TEST_F(PaymentsClientTest, UploadIncludesNonLocationData) {
  StartUploading(UploadCardOptions());
  IssueOAuthToken();

  // Verify that the recipient name field and test names do appear in the upload
  // data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kRecipientName) !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("John") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Smith") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Pat") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("Jones") != std::string::npos);

  // Verify that the phone number field and test numbers do appear in the upload
  // data.
  EXPECT_TRUE(GetUploadData().find(PaymentsClient::kPhoneNumber) !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("212") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("555") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0162") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("834") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("0090") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       UploadRequestIncludesBillingCustomerNumberInRequest) {
  StartUploading(UploadCardOptions().with_billing_customer_number(1234L));
  IssueOAuthToken();

  // Verify that the billing customer number is included in the request.
  EXPECT_TRUE(GetUploadData().find("%22external_customer_id%22:%221234%22") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest,
       UploadRequestExcludesBillingCustomerNumberIfNoBcnExists) {
  // A value of zero is treated as a non-existent BCN.
  StartUploading(UploadCardOptions().with_billing_customer_number(0L));
  IssueOAuthToken();

  // Verify that the billing customer number is not included in the request if
  // billing customer number is 0.
  EXPECT_TRUE(GetUploadData().find("\"external_customer_id\"") ==
              std::string::npos);
}

TEST_F(PaymentsClientTest, UploadRequestIncludesClientBehaviorSignals) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnableNewSaveCardBubbleUi);

  StartUploading(UploadCardOptions().with_client_behavior_signals(
      std::vector<ClientBehaviorConstants>{
          ClientBehaviorConstants::kUsingFasterAndProtectedUi}));
  IssueOAuthToken();

  // Verify ChromeUserContext was set.
  EXPECT_THAT(GetUploadData(), HasSubstr("chrome_user_context"));
  // Verify Client_behavior_signals was set.
  EXPECT_THAT(GetUploadData(), HasSubstr("client_behavior_signals"));
  // Verify fake_client_behavior_signal was set.
  // ClientBehaviorConstants::kUsingFasterAndProtectedUi has the numeric value
  // set to 1.
  EXPECT_THAT(GetUploadData(),
              HasSubstr("%22client_behavior_signals%22:%5B1%5D"));
}

TEST_F(PaymentsClientTest, UploadRequestIncludesPan) {
  StartUploading(UploadCardOptions());
  IssueOAuthToken();

  // Verify that the `pan` and s7e_21_pan parameters were included in the
  // request, and the legacy field `encrypted_pan` was not.
  EXPECT_TRUE(GetUploadData().find("encrypted_pan") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("pan") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("__param:s7e_21_pan") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("&s7e_21_pan=4111111111111111") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, UploadIncludesCvcInRequestIfProvided) {
  StartUploading(UploadCardOptions().with_cvc_in_request(true));
  IssueOAuthToken();

  // Verify that the encrypted_cvc and s7e_13_cvc parameters were included in
  // the request.
  EXPECT_TRUE(GetUploadData().find("encrypted_cvc") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("__param:s7e_13_cvc") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("&s7e_13_cvc=") != std::string::npos);
}

TEST_F(PaymentsClientTest, UploadDoesNotIncludeCvcInRequestIfNotProvided) {
  StartUploading(UploadCardOptions().with_cvc_in_request(false));
  IssueOAuthToken();

  EXPECT_TRUE(!GetUploadData().empty());
  // Verify that the encrypted_cvc and s7e_13_cvc parameters were not included
  // in the request.
  EXPECT_TRUE(GetUploadData().find("encrypted_cvc") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("__param:s7e_13_cvc") == std::string::npos);
  EXPECT_TRUE(GetUploadData().find("&s7e_13_cvc=") == std::string::npos);
}

TEST_F(PaymentsClientTest, UploadIncludesChromeUserContext) {
  StartUploading(UploadCardOptions());
  IssueOAuthToken();

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest, UploadIncludesCardNickname) {
  StartUploading(UploadCardOptions().with_nickname_in_request(true));
  IssueOAuthToken();

  // Card nickname was set.
  EXPECT_TRUE(GetUploadData().find("nickname") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find(base::UTF16ToUTF8(upstream_nickname_)) !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, UploadDoesNotIncludeCardNicknameEmptyNickname) {
  StartUploading(UploadCardOptions().with_nickname_in_request(false));
  IssueOAuthToken();

  // Card nickname was not set.
  EXPECT_FALSE(GetUploadData().find("nickname") != std::string::npos);
}

TEST_F(PaymentsClientTest, UnmaskMissingPan) {
  StartUnmasking(CardUnmaskOptions());
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
}

TEST_F(PaymentsClientTest, UnmaskRetryFailure) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"error\": { \"code\": \"INTERNAL\" } }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kTryAgainFailure, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsClientTest, UnmaskPermanentFailure) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsClientTest, UnmaskMalformedResponse) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"error_code\": \"WRONG_JSON_FORMAT\" }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsClientTest, ReauthNeeded) {
  {
    StartUnmasking(CardUnmaskOptions());
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::PaymentsRpcResult::kNone, result_);
    EXPECT_EQ(nullptr, unmask_response_details());

    // Second HTTP_UNAUTHORIZED causes permanent failure.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
    EXPECT_EQ("", unmask_response_details()->real_pan);
  }

  result_ = AutofillClient::PaymentsRpcResult::kNone;
  ResetUnmaskResponseDetails();

  {
    StartUnmasking(CardUnmaskOptions());
    // NOTE: Don't issue an access token here: the issuing of an access token
    // first waits for the access token request to be received, but here there
    // should be no access token request because PaymentsClient should reuse the
    // access token from the previous request.
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::PaymentsRpcResult::kNone, result_);
    EXPECT_EQ(nullptr, unmask_response_details());

    // HTTP_OK after first HTTP_UNAUTHORIZED results in success.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
    EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
    EXPECT_EQ("1234", unmask_response_details()->real_pan);
  }
}

TEST_F(PaymentsClientTest, NetworkError) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_REQUEST_TIMEOUT, std::string());
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kNetworkError, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsClientTest, OtherError) {
  StartUnmasking(CardUnmaskOptions());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_FORBIDDEN, std::string());
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ("", unmask_response_details()->real_pan);
}

TEST_F(PaymentsClientTest, VcnRetrievalTryAgainFailure) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_temporary_error\" } }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
            result_);
}

TEST_F(PaymentsClientTest, VcnRetrievalPermanentFailure) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure,
            result_);
}

TEST_F(PaymentsClientTest, UnmaskPermanentFailureWhenVcnMissingExpiration) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"dcvv\": \"999\" }");

  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
}

TEST_F(PaymentsClientTest, UnmaskPermanentFailureWhenVcnMissingCvv) {
  StartUnmasking(CardUnmaskOptions().with_virtual_card());
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"pan\": \"4111111111111111\", \"expiration\": { "
                 "\"month\":12, \"year\":2099 } }");

  EXPECT_EQ("4111111111111111", unmask_response_details()->real_pan);
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
}

// Tests for the local card migration flow. Desktop only.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PaymentsClientTest, GetDetailsFollowedByMigrationSuccess) {
  StartGettingUploadDetails(GetUploadDetailsOptions());
  IssueOAuthToken();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);

  result_ = AutofillClient::PaymentsRpcResult::kNone;

  StartMigrating(/*has_cardholder_name=*/true);
  ReturnResponse(
      net::HTTP_OK,
      "{\"save_result\":[],\"value_prop_display_text\":\"display text\"}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
}
#endif

// Tests for the local card migration flow. Desktop only.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PaymentsClientTest, MigrateCardsVariationsTest) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("AutofillTest", "Group", 369);
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // Note that experiment information is stored in X-Client-Data.
  EXPECT_TRUE(HasVariationsHeader());
}

TEST_F(PaymentsClientTest, MigrationRequestIncludesUniqueId) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // Verify that the unique id was included in the request.
  EXPECT_TRUE(GetUploadData().find("unique_id") != std::string::npos);
  EXPECT_TRUE(
      GetUploadData().find(migratable_credit_cards_[0].credit_card().guid()) !=
      std::string::npos);
  EXPECT_TRUE(
      GetUploadData().find(migratable_credit_cards_[1].credit_card().guid()) !=
      std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationRequestIncludesEncryptedPan) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // Verify that the encrypted_pan and s7e_1_pan parameters were included
  // in the request.
  EXPECT_TRUE(GetUploadData().find("encrypted_pan") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("__param:s7e_1_pan0") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("&s7e_1_pan0=4111111111111111") !=
              std::string::npos);
  EXPECT_TRUE(GetUploadData().find("__param:s7e_1_pan1") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("&s7e_1_pan1=378282246310005") !=
              std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationRequestIncludesCardholderNameWhenItExists) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  EXPECT_TRUE(!GetUploadData().empty());
  // Verify that the cardholder name is sent if it exists.
  EXPECT_TRUE(GetUploadData().find("cardholder_name") != std::string::npos);
}

TEST_F(PaymentsClientTest,
       MigrationRequestExcludesCardholderNameWhenItDoesNotExist) {
  StartMigrating(/*has_cardholder_name=*/false);
  IssueOAuthToken();

  EXPECT_TRUE(!GetUploadData().empty());
  // Verify that the cardholder name is not sent if it doesn't exist.
  EXPECT_TRUE(GetUploadData().find("cardholder_name") == std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationRequestIncludesChromeUserContext) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();

  // ChromeUserContext was set.
  EXPECT_TRUE(GetUploadData().find("chrome_user_context") != std::string::npos);
  EXPECT_TRUE(GetUploadData().find("full_sync_enabled") != std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationRequestIncludesCardNickname) {
  StartMigrating(/*has_cardholder_name=*/true,
                 /*set_nickname_for_first_card=*/true);
  IssueOAuthToken();

  // Nickname was set for the first card.
  std::size_t pos = GetUploadData().find("nickname");
  EXPECT_TRUE(pos != std::string::npos);
  EXPECT_TRUE(GetUploadData().find(base::UTF16ToUTF8(
                  migratable_credit_cards_[0].credit_card().nickname())) !=
              std::string::npos);

  // Nickname was not set for the second card.
  EXPECT_FALSE(GetUploadData().find("nickname", pos + 1) != std::string::npos);
}

TEST_F(PaymentsClientTest, MigrationSuccessWithSaveResult) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{\"save_result\":[{\"unique_id\":\"0\",\"status\":"
                 "\"SUCCESS\"},{\"unique_id\":\"1\",\"status\":\"TEMPORARY_"
                 "FAILURE\"}],\"value_prop_display_text\":\"display text\"}");

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_TRUE(migration_save_results_.get());
  EXPECT_TRUE(migration_save_results_->find("0") !=
              migration_save_results_->end());
  EXPECT_TRUE(migration_save_results_->at("0") == "SUCCESS");
  EXPECT_TRUE(migration_save_results_->find("1") !=
              migration_save_results_->end());
  EXPECT_TRUE(migration_save_results_->at("1") == "TEMPORARY_FAILURE");
}

TEST_F(PaymentsClientTest, MigrationMissingSaveResult) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{\"value_prop_display_text\":\"display text\"}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
  EXPECT_EQ(nullptr, migration_save_results_.get());
}

TEST_F(PaymentsClientTest, MigrationSuccessWithDisplayText) {
  StartMigrating(/*has_cardholder_name=*/true);
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{\"save_result\":[{\"unique_id\":\"0\",\"status\":"
                 "\"SUCCESS\"}],\"value_prop_display_text\":\"display text\"}");
  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("display text", display_text_);
}
#endif

TEST_F(PaymentsClientTest, SelectChallengeOptionWithSmsOtpMethod) {
  StartSelectingChallengeOption(CardUnmaskChallengeOptionType::kSmsOtp,
                                "arbitrary id for sms otp");
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"context_token\": \"new context token\" }");

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
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

TEST_F(PaymentsClientTest, SelectChallengeOptionSuccess) {
  StartSelectingChallengeOption();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"context_token\": \"new context token\" }");

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kSuccess, result_);
  EXPECT_EQ("new context token", context_token_);
}

TEST_F(PaymentsClientTest, SelectChallengeOptionTemporaryFailure) {
  StartSelectingChallengeOption();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_temporary_error\"} }");

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
            result_);
}

TEST_F(PaymentsClientTest, SelectChallengeOptionVcnFlowPermanentFailure) {
  StartSelectingChallengeOption();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
                 "\"api_error_reason\": \"virtual_card_permanent_error\"} }");

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure,
            result_);
}

TEST_F(PaymentsClientTest, SelectChallengeOptionResponseMissingContextToken) {
  StartSelectingChallengeOption();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");

  EXPECT_EQ(AutofillClient::PaymentsRpcResult::kPermanentFailure, result_);
}

typedef std::tuple<VirtualCardEnrollmentSource,
                   VirtualCardEnrollmentRequestType,
                   AutofillClient::PaymentsRpcResult>
    UpdateVirtualCardEnrollmentTestData;

class UpdateVirtualCardEnrollmentTest
    : public PaymentsClientTest,
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

    // |response_type_for_test| is the AutofillClient::PaymentsRpcResult
    // response type we want to test for the combination of
    // |virtual_card_enrollment_source| and
    // |virtual_card_enrollment_request_type| we are currently on.
    AutofillClient::PaymentsRpcResult response_type_for_test =
        std::get<2>(GetParam());
    switch (response_type_for_test) {
      case AutofillClient::PaymentsRpcResult::kSuccess:
        if (virtual_card_enrollment_request_type ==
            VirtualCardEnrollmentRequestType::kEnroll) {
          ReturnResponse(net::HTTP_OK,
                         "{ \"enroll_result\": \"ENROLL_SUCCESS\" }");
        } else if (virtual_card_enrollment_request_type ==
                   VirtualCardEnrollmentRequestType::kUnenroll) {
          ReturnResponse(net::HTTP_OK, "{}");
        }
        break;
      case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
        ReturnResponse(
            net::HTTP_OK,
            "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
            "\"api_error_reason\": \"virtual_card_temporary_error\"} }");
        break;
      case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
        ReturnResponse(net::HTTP_OK,
                       "{ \"error\": { \"code\": \"INTERNAL\", "
                       "\"api_error_reason\": \"ANYTHING_ELSE\"} }");
        break;
      case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
        ReturnResponse(
            net::HTTP_OK,
            "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
            "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
        break;
      case AutofillClient::PaymentsRpcResult::kPermanentFailure:
        ReturnResponse(net::HTTP_OK,
                       "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
        break;
      case AutofillClient::PaymentsRpcResult::kNetworkError:
        ReturnResponse(net::HTTP_REQUEST_TIMEOUT, "");
        break;
      case AutofillClient::PaymentsRpcResult::kNone:
        NOTREACHED();
        break;
    }
    EXPECT_EQ(response_type_for_test, result_);
  }

 private:
  void StartUpdateVirtualCardEnrollment(
      VirtualCardEnrollmentSource virtual_card_enrollment_source,
      VirtualCardEnrollmentRequestType virtual_card_enrollment_request_type) {
    PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails request_details;
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
    client_->UpdateVirtualCardEnrollment(
        request_details,
        base::BindOnce(
            &PaymentsClientTest::OnDidGetUpdateVirtualCardEnrollmentResponse,
            GetWeakPtr()));
  }
};

// Initializes the parameterized test suite with all possible values of
// VirtualCardEnrollmentSource, VirtualCardEnrollmentRequestType, and
// AutofillClient::PaymentsRpcResult.
INSTANTIATE_TEST_SUITE_P(
    ,
    UpdateVirtualCardEnrollmentTest,
    testing::Combine(
        testing::Values(VirtualCardEnrollmentSource::kUpstream,
                        VirtualCardEnrollmentSource::kDownstream,
                        VirtualCardEnrollmentSource::kSettingsPage),
        testing::Values(VirtualCardEnrollmentRequestType::kEnroll,
                        VirtualCardEnrollmentRequestType::kUnenroll),
        testing::Values(
            AutofillClient::PaymentsRpcResult::kSuccess,
            AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
            AutofillClient::PaymentsRpcResult::kTryAgainFailure,
            AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure,
            AutofillClient::PaymentsRpcResult::kPermanentFailure,
            AutofillClient::PaymentsRpcResult::kNetworkError)));

// Parameterized test that tests all combinations of
// VirtualCardEnrollmentSource and VirtualCardEnrollmentRequestType against all
// possible server responses in the UpdateVirtualCardEnrollmentFlow. This test
// will be run once for each combination.
TEST_P(UpdateVirtualCardEnrollmentTest,
       UpdateVirtualCardEnrollmentTest_TestAllFlows) {
  TriggerFlow();
}

class GetVirtualCardEnrollmentDetailsTest
    : public PaymentsClientTest,
      public ::testing::WithParamInterface<
          std::tuple<VirtualCardEnrollmentSource,
                     AutofillClient::PaymentsRpcResult>> {
 public:
  GetVirtualCardEnrollmentDetailsTest() = default;
  ~GetVirtualCardEnrollmentDetailsTest() override = default;
};

// Initializes the parameterized test suite with all possible combinations of
// VirtualCardEnrollmentSource and AutofillClient::PaymentsRpcResult.
INSTANTIATE_TEST_SUITE_P(
    ,
    GetVirtualCardEnrollmentDetailsTest,
    testing::Combine(
        testing::Values(VirtualCardEnrollmentSource::kUpstream,
                        VirtualCardEnrollmentSource::kDownstream,
                        VirtualCardEnrollmentSource::kSettingsPage),
        testing::Values(
            AutofillClient::PaymentsRpcResult::kSuccess,
            AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure,
            AutofillClient::PaymentsRpcResult::kTryAgainFailure,
            AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure,
            AutofillClient::PaymentsRpcResult::kPermanentFailure,
            AutofillClient::PaymentsRpcResult::kNetworkError)));

// Parameterized test that tests all combinations of
// VirtualCardEnrollmentSource and server PaymentsRpcResult. This test
// will be run once for each combination.
TEST_P(GetVirtualCardEnrollmentDetailsTest,
       GetVirtualCardEnrollmentDetailsTest_TestAllFlows) {
  VirtualCardEnrollmentSource source = std::get<0>(GetParam());

  PaymentsClient::GetDetailsForEnrollmentRequestDetails request_details;
  request_details.source = source;
  request_details.instrument_id = 12345678;
  request_details.billing_customer_number = 555666777888;
  request_details.risk_data = "fake risk data";
  request_details.app_locale = "en";

  client_->GetVirtualCardEnrollmentDetails(
      request_details,
      base::BindOnce(&PaymentsClientTest::OnDidGetVirtualCardEnrollmentDetails,
                     GetWeakPtr()));
  IssueOAuthToken();

  // Ensures the PaymentsRpcResult is set correctly.
  AutofillClient::PaymentsRpcResult result = std::get<1>(GetParam());
  switch (result) {
    case AutofillClient::PaymentsRpcResult::kSuccess:
      ReturnResponse(
          net::HTTP_OK,
          "{ \"google_legal_message\": { \"line\" : [{ \"template\": \"This is "
          "the entire message.\" }] }, \"external_legal_message\": {}, "
          "\"context_token\": \"some_token\" }");
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure:
      ReturnResponse(
          net::HTTP_OK,
          "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
          "\"api_error_reason\": \"virtual_card_temporary_error\"} }");
      break;
    case AutofillClient::PaymentsRpcResult::kTryAgainFailure:
      ReturnResponse(net::HTTP_OK,
                     "{ \"error\": { \"code\": \"INTERNAL\", "
                     "\"api_error_reason\": \"ANYTHING_ELSE\"} }");
      break;
    case AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure:
      ReturnResponse(
          net::HTTP_OK,
          "{ \"error\": { \"code\": \"ANYTHING_ELSE\", "
          "\"api_error_reason\": \"virtual_card_permanent_error\"} }");
      break;
    case AutofillClient::PaymentsRpcResult::kPermanentFailure:
      ReturnResponse(net::HTTP_OK,
                     "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
      break;
    case AutofillClient::PaymentsRpcResult::kNetworkError:
      ReturnResponse(net::HTTP_REQUEST_TIMEOUT, "");
      break;
    case AutofillClient::PaymentsRpcResult::kNone:
      NOTREACHED();
      break;
  }
  EXPECT_EQ(result, result_);
}

}  // namespace autofill::payments
