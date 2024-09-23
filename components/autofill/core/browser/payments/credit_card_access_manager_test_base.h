// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_TEST_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_TEST_BASE_H_

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/sync/test/test_sync_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace payments {
class TestPaymentsNetworkInterface;
}  // namespace payments

class CreditCard;
class CreditCardCvcAuthenticator;
class TestAutofillDriver;
class TestCreditCardOtpAuthenticator;
class TestPersonalDataManager;

struct CardUnmaskChallengeOption;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
class TestCreditCardFidoAuthenticator;
#endif

// A base class for unittests for CreditCardAccessManager, containing logic and
// state that is shared across multiple test classes.
class CreditCardAccessManagerTestBase : public testing::Test {
 public:
  static constexpr char kTestGUID[] = "00000000-0000-0000-0000-000000000001";
  static constexpr char kTestGUID2[] = "00000000-0000-0000-0000-000000000002";
  static constexpr char kTestNumber[] = "4234567890123456";  // Visa
  static constexpr char kTestNumber2[] = "5454545454545454";
  static constexpr char16_t kTestNumber16[] = u"4234567890123456";
  static constexpr char16_t kTestCvc16[] = u"123";
  static constexpr char kTestServerId[] = "server_id_1";
  static constexpr char kTestServerId2[] = "server_id_2";

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  static constexpr char kTestCvc[] = "123";
  // Base64 encoding of "This is a test challenge".
  static constexpr char kTestChallenge[] = "VGhpcyBpcyBhIHRlc3QgY2hhbGxlbmdl";
  // Base64 encoding of "This is a test Credential ID".
  static constexpr char kCredentialId[] =
      "VGhpcyBpcyBhIHRlc3QgQ3JlZGVudGlhbCBJRC4=";
  static constexpr char kGooglePaymentsRpid[] = "google.com";
#endif

  // Implements an `OnCreditCardFetched()` callback that stores the values it
  // receives.
  class TestAccessor {
   public:
    TestAccessor();
    ~TestAccessor();

    void OnCreditCardFetched(CreditCardFetchResult result,
                             const CreditCard* card);

    base::WeakPtr<TestAccessor> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

    std::u16string number() { return number_; }
    std::u16string cvc() { return cvc_; }
    std::u16string expiry_month() { return expiry_month_; }
    std::u16string expiry_year() { return expiry_year_; }
    CreditCardFetchResult result() { return result_; }

   private:
    // The result of the credit card fetching.
    CreditCardFetchResult result_ = CreditCardFetchResult::kNone;
    // The card number returned from OnCreditCardFetched().
    std::u16string number_;
    // The returned CVC, if any.
    std::u16string cvc_;
    // The two-digit expiration month in string.
    std::u16string expiry_month_;
    // The four-digit expiration year in string.
    std::u16string expiry_year_;
    base::WeakPtrFactory<TestAccessor> weak_ptr_factory_{this};
  };

  // The type of request options to be returned with a CVC auth response.
  enum class TestFidoRequestOptionsType {
    kValid = 0,
    kInvalid = 1,
    kNotPresent = 2
  };

  CreditCardAccessManagerTestBase();
  ~CreditCardAccessManagerTestBase() override;

  void SetUp() override;

  bool IsAuthenticationInProgress();

  // Resets all variables related to credit card fetching.
  void ResetFetchCreditCard();

  void ClearCards();

  void CreateLocalCard(std::string guid, std::string number = std::string());

  CreditCard* CreateServerCard(std::string guid,
                               std::string number = std::string(),
                               std::string server_id = std::string());

  CreditCardCvcAuthenticator& GetCvcAuthenticator();

  void MockUserResponseForCvcAuth(std::u16string cvc, bool enable_fido);

  // Returns true if full card request was sent from CVC auth.
  bool GetRealPanForCVCAuth(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const std::string& real_pan,
      TestFidoRequestOptionsType test_fido_request_options_type =
          TestFidoRequestOptionsType::kNotPresent);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  void AddMaxStrikes();
  void ClearStrikes();
  int GetStrikes();

  base::Value::Dict GetTestRequestOptions(
      bool return_invalid_request_options = false);
  base::Value::Dict GetTestCreationOptions();

  // Returns true if full card request was sent from FIDO auth.
  bool GetRealPanForFIDOAuth(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const std::string& real_pan,
      const std::string& dcvv = std::string(),
      bool is_virtual_card = false);

  // Mocks an OptChange response from the PaymentsNetworkInterface.
  void OptChange(payments::PaymentsAutofillClient::PaymentsRpcResult result,
                 bool user_is_opted_in,
                 bool include_creation_options = false,
                 bool include_request_options = false);

  TestCreditCardFidoAuthenticator* GetFIDOAuthenticator();
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // Mocks user response for the offer dialog.
  void AcceptWebauthnOfferDialog(bool did_accept);
#endif

  void InvokeDelayedGetUnmaskDetailsResponse();
  void InvokeUnmaskDetailsTimeout();
  void WaitForCallbacks();

  void SetCreditCardFIDOAuthEnabled(bool enabled);
  bool IsCreditCardFIDOAuthEnabled();

  UnmaskAuthFlowType GetUnmaskAuthFlowType();

  void MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      bool fido_authenticator_is_user_opted_in,
      bool is_user_verifiable,
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      int selected_index);

  void VerifyOnSelectChallengeOptionInvoked();

 protected:
  CreditCardAccessManager& credit_card_access_manager();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  TestCreditCardFidoAuthenticator& fido_authenticator();
#endif
  payments::TestPaymentsNetworkInterface& payments_network_interface();
  TestPersonalDataManager& personal_data();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  void OptUserInToFido();
#endif

  std::unique_ptr<TestAccessor> accessor_;
  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  raw_ptr<TestCreditCardOtpAuthenticator> otp_authenticator_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_ACCESS_MANAGER_TEST_BASE_H_
