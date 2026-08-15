// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/record_legal_reminder_acknowledgment_request.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

namespace {

constexpr char kAppLocale[] = "en-US";
constexpr int kBillableServiceNumber = 1234;
constexpr int64_t kBillingCustomerNumber = 111222333;
constexpr char kTrackedLegalMessageToken[] = "test_token";

class RecordLegalReminderAcknowledgmentRequestTest : public testing::Test {
 public:
  void SetUp() override {
    RecordLegalReminderAcknowledgmentRequestDetails request_details;
    request_details.app_locale = kAppLocale;
    request_details.billable_service_number = kBillableServiceNumber;
    request_details.billing_customer_number = kBillingCustomerNumber;
    request_details.legal_message_token = kTrackedLegalMessageToken;
    request_ = std::make_unique<RecordLegalReminderAcknowledgmentRequest>(
        request_details, base::DoNothing());
  }

  RecordLegalReminderAcknowledgmentRequest* GetRequest() {
    return request_.get();
  }

  void ParseResponse(const base::DictValue& response) {
    request_->ParseResponse(response);
  }

  bool IsResponseComplete() const { return request_->IsResponseComplete(); }

 private:
  std::unique_ptr<RecordLegalReminderAcknowledgmentRequest> request_;
};

TEST_F(RecordLegalReminderAcknowledgmentRequestTest,
       GetRequestContent_ContainsExpectedData) {
  EXPECT_EQ(
      GetRequest()->GetRequestUrlPath(),
      "payments/apis/chromepaymentsservice/recordlegalreminderacknowledgment");
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
  EXPECT_NE(request_content.find("legal_message_token"),
            std::string::npos);
  EXPECT_NE(request_content.find(kTrackedLegalMessageToken), std::string::npos);
}

TEST_F(RecordLegalReminderAcknowledgmentRequestTest, ParseResponse) {
  base::DictValue response;
  ParseResponse(response);

  EXPECT_TRUE(IsResponseComplete());
}

}  // namespace

}  // namespace autofill::payments
