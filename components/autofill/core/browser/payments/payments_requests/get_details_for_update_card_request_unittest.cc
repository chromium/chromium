// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_details_for_update_card_request.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_update_card_request_test_api.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

constexpr int64_t kTestBillingCustomerNumber = 1234567890;
constexpr int64_t kTestInstrumentId = 987654321;
constexpr char kTestAppLocale[] = "en-US";
constexpr char kTestContextToken[] = "test_context_token_12345";

class GetDetailsForUpdateCardRequestTest : public testing::Test {
 public:
  void SetUp() override {
    request_details_.app_locale = kTestAppLocale;
    request_details_.billing_customer_number = kTestBillingCustomerNumber;
    request_details_.instrument_id = kTestInstrumentId;
  }

  std::unique_ptr<GetDetailsForUpdateCardRequest> CreateRequest() {
    return std::make_unique<GetDetailsForUpdateCardRequest>(
        request_details_, mock_callback_.Get());
  }

 protected:
  GetDetailsForUpdateCardRequestDetails request_details_;
  base::MockOnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                              const std::string&)>
      mock_callback_;
};

TEST_F(GetDetailsForUpdateCardRequestTest, GetRequestUrlPath) {
  std::unique_ptr<GetDetailsForUpdateCardRequest> request = CreateRequest();
  EXPECT_EQ(request->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/"
            "getdetailsforupdatepaymentinstrument");
}

TEST_F(GetDetailsForUpdateCardRequestTest, GetRequestContentType) {
  std::unique_ptr<GetDetailsForUpdateCardRequest> request = CreateRequest();
  EXPECT_EQ(request->GetRequestContentType(), "application/json");
}

TEST_F(GetDetailsForUpdateCardRequestTest, GetRequestContent) {
  std::unique_ptr<GetDetailsForUpdateCardRequest> request = CreateRequest();

  base::DictValue request_dict =
      base::test::ParseJsonDict(request->GetRequestContent());

  // Verify context.
  const base::DictValue* context = request_dict.FindDict("context");
  ASSERT_TRUE(context);
  EXPECT_EQ(*context->FindString("language_code"), kTestAppLocale);
  EXPECT_EQ(context->FindInt("billable_service"),
            kUploadPaymentMethodBillableServiceNumber);
  const base::DictValue* customer_context =
      context->FindDict("customer_context");
  ASSERT_TRUE(customer_context);
  EXPECT_EQ(*customer_context->FindString("external_customer_id"),
            base::NumberToString(kTestBillingCustomerNumber));

  // Verify chrome_user_context is present.
  const base::DictValue* chrome_user_context =
      request_dict.FindDict("chrome_user_context");
  ASSERT_TRUE(chrome_user_context);

  // Verify instrument_id.
  EXPECT_EQ(*request_dict.FindString("instrument_id"),
            base::NumberToString(kTestInstrumentId));

  // Verify card_info is present and is an empty dictionary.
  const base::DictValue* card_info = request_dict.FindDict("card_info");
  ASSERT_TRUE(card_info);
  EXPECT_TRUE(card_info->empty());
}

TEST_F(GetDetailsForUpdateCardRequestTest,
       ParseResponse_SuccessWithContextToken) {
  std::unique_ptr<GetDetailsForUpdateCardRequest> request = CreateRequest();

  base::DictValue response;
  response.Set("context_token", kTestContextToken);

  request->ParseResponse(response);

  EXPECT_TRUE(request->IsResponseComplete());
  EXPECT_EQ(test_api(*request).get_context_token(), kTestContextToken);
}

TEST_F(GetDetailsForUpdateCardRequestTest,
       ParseResponse_MissingContextToken_Incomplete) {
  std::unique_ptr<GetDetailsForUpdateCardRequest> request = CreateRequest();

  base::DictValue response;

  request->ParseResponse(response);

  EXPECT_FALSE(request->IsResponseComplete());
}

TEST_F(GetDetailsForUpdateCardRequestTest,
       ParseResponse_EmptyContextToken_Incomplete) {
  std::unique_ptr<GetDetailsForUpdateCardRequest> request = CreateRequest();

  base::DictValue response;
  response.Set("context_token", "");

  request->ParseResponse(response);

  EXPECT_FALSE(request->IsResponseComplete());
}

TEST_F(GetDetailsForUpdateCardRequestTest, RespondToDelegate) {
  std::unique_ptr<GetDetailsForUpdateCardRequest> request = CreateRequest();

  base::DictValue response;
  response.Set("context_token", kTestContextToken);
  request->ParseResponse(response);

  EXPECT_CALL(mock_callback_,
              Run(PaymentsAutofillClient::PaymentsRpcResult::kSuccess,
                  kTestContextToken));

  request->RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult::kSuccess);
}

}  // namespace
}  // namespace autofill::payments
