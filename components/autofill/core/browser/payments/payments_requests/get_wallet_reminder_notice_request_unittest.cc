// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_wallet_reminder_notice_request.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/payments_requests/get_wallet_reminder_notice_request_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {

constexpr char kAppLocale[] = "en-US";
constexpr int kBillableServiceNumber = 1234;
constexpr int64_t kBillingCustomerNumber = 111222333;

MATCHER_P(HasLegalMessageLineText, text, "A LegalMessageLine that has text.") {
  return base::UTF16ToUTF8(arg.text()) == text;
}

class GetWalletReminderNoticeRequestTest : public testing::Test {
 public:
  void SetUp() override {
    GetWalletReminderNoticeRequestDetails request_details;
    request_details.app_locale = kAppLocale;
    request_details.billable_service_number = kBillableServiceNumber;
    request_details.billing_customer_number = kBillingCustomerNumber;
    request_ = std::make_unique<GetWalletReminderNoticeRequest>(
        request_details, base::DoNothing());
  }

  GetWalletReminderNoticeRequest* GetRequest() { return request_.get(); }

  void ParseResponse(const base::DictValue& response) {
    request_->ParseResponse(response);
  }

  bool IsResponseComplete() const { return request_->IsResponseComplete(); }

 private:
  std::unique_ptr<GetWalletReminderNoticeRequest> request_;
};

TEST_F(GetWalletReminderNoticeRequestTest,
       GetRequestContent_ContainsExpectedData) {
  EXPECT_EQ(GetRequest()->GetRequestUrlPath(),
            "payments/apis/chromepaymentsservice/getwalletremindernotice");
  EXPECT_EQ(GetRequest()->GetRequestContentType(), "application/json");

  std::string request_content = GetRequest()->GetRequestContent();
  ASSERT_FALSE(request_content.empty());
  EXPECT_NE(request_content.find("language_code"), std::string::npos);
  EXPECT_NE(request_content.find(kAppLocale), std::string::npos);
  EXPECT_NE(request_content.find("billable_service"), std::string::npos);
  EXPECT_NE(request_content.find(base::NumberToString(kBillableServiceNumber)),
            std::string::npos);
  EXPECT_NE(request_content.find("customer_context"), std::string::npos);
  EXPECT_NE(request_content.find("external_customer_id"), std::string::npos);
  EXPECT_NE(request_content.find(base::NumberToString(kBillingCustomerNumber)),
            std::string::npos);
}

TEST_F(
    GetWalletReminderNoticeRequestTest,
    ParseResponse_UserNotShownReminder_LegalMessagePresent_ResponseIsComplete) {
  base::DictValue response =
      base::DictValue()
          .Set("has_user_been_shown_reminder", false)
          .Set("legal_message",
               base::DictValue()
                   .Set("line", base::ListValue().Append(base::DictValue().Set(
                                    "template", "some legal message line")))
                   .Set("token", "some_token"));

  ParseResponse(response);

  EXPECT_FALSE(
      test_api(*GetRequest()).response_details().has_user_been_shown_reminder);
  EXPECT_EQ(test_api(*GetRequest()).response_details().acknowledgement_token,
            "some_token");
  EXPECT_THAT(
      test_api(*GetRequest()).response_details().legal_message_lines,
      testing::ElementsAre(HasLegalMessageLineText("some legal message line")));
  EXPECT_TRUE(IsResponseComplete());
}

TEST_F(GetWalletReminderNoticeRequestTest,
       ParseResponse_UserAlreadyShownReminder_ResponseIsComplete) {
  base::DictValue response =
      base::DictValue().Set("has_user_been_shown_reminder", true);

  ParseResponse(response);

  EXPECT_TRUE(
      test_api(*GetRequest()).response_details().has_user_been_shown_reminder);
  EXPECT_TRUE(IsResponseComplete());
}

TEST_F(
    GetWalletReminderNoticeRequestTest,
    ParseResponse_UserNotShownReminder_MissingLegalMessageToken_ResponseIsIncomplete) {
  base::DictValue response =
      base::DictValue()
          .Set("has_user_been_shown_reminder", false)
          .Set("legal_message",
               base::DictValue().Set(
                   "line", base::ListValue().Append(base::DictValue().Set(
                               "template", "some legal message line"))));

  ParseResponse(response);

  EXPECT_FALSE(
      test_api(*GetRequest()).response_details().has_user_been_shown_reminder);
  EXPECT_THAT(
      test_api(*GetRequest()).response_details().legal_message_lines,
      testing::ElementsAre(HasLegalMessageLineText("some legal message line")));
  EXPECT_FALSE(IsResponseComplete());
}

TEST_F(
    GetWalletReminderNoticeRequestTest,
    ParseResponse_UserNotShownReminder_MissingLegalMessageLines_ResponseIsIncomplete) {
  base::DictValue response =
      base::DictValue()
          .Set("has_user_been_shown_reminder", false)
          .Set("legal_message", base::DictValue().Set("token", "some_token"));

  ParseResponse(response);

  EXPECT_FALSE(
      test_api(*GetRequest()).response_details().has_user_been_shown_reminder);
  EXPECT_EQ(test_api(*GetRequest()).response_details().acknowledgement_token,
            "some_token");
  EXPECT_FALSE(IsResponseComplete());
}

}  // namespace

}  // namespace autofill::payments
