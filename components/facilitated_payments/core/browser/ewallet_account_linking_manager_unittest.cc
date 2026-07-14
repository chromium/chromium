// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/payments/test_payments_data_manager.h"
#include "components/facilitated_payments/core/browser/ewallet_account_linking_manager_test_api.h"
#include "components/facilitated_payments/core/browser/mock_facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/network_api/mock_facilitated_payments_network_interface.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

using ::testing::_;

class EwalletAccountLinkingManagerTest : public testing::Test {
 public:
  EwalletAccountLinkingManagerTest() {
    manager_ = std::make_unique<EwalletAccountLinkingManager>(
        &client_, /*api_client_creator=*/base::BindRepeating(
            []() -> std::unique_ptr<FacilitatedPaymentsApiClient> {
              return nullptr;
            }));

    payments_network_interface_ =
        std::make_unique<MockFacilitatedPaymentsNetworkInterface>(
            *identity_test_env_.identity_manager(), payments_data_manager_);
  }

  void SetUp() override {
    ON_CALL(client_, GetFacilitatedPaymentsNetworkInterface)
        .WillByDefault(testing::Return(payments_network_interface_.get()));
    ON_CALL(client_, GetPaymentsDataManager)
        .WillByDefault(testing::Return(&payments_data_manager_));
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockFacilitatedPaymentsClient client_;
  signin::IdentityTestEnvironment identity_test_env_;
  autofill::TestPaymentsDataManager payments_data_manager_;
  std::unique_ptr<MockFacilitatedPaymentsNetworkInterface>
      payments_network_interface_;
  std::unique_ptr<EwalletAccountLinkingManager> manager_;
};

TEST_F(EwalletAccountLinkingManagerTest, GetHistogramSuffix) {
  EXPECT_EQ("Ewallet", test_api(*manager_).GetHistogramSuffix());
}

TEST_F(EwalletAccountLinkingManagerTest,
       GetPayloadForGetDetailsForCreatePaymentInstrument) {
  base::DictValue expected_payload;
  EXPECT_EQ(
      expected_payload,
      test_api(*manager_).GetPayloadForGetDetailsForCreatePaymentInstrument());
}

TEST_F(EwalletAccountLinkingManagerTest, DoOnAccountLinkingResult) {
  // Just verify it doesn't crash.
  test_api(*manager_).DoOnAccountLinkingResult(AccountLinkingResult{true});
  test_api(*manager_).DoOnAccountLinkingResult(AccountLinkingResult{false});
}

TEST_F(EwalletAccountLinkingManagerTest, DoOnClientTokenReceived) {
  std::vector<uint8_t> client_token = {'t', 'o', 'k', 'e', 'n'};

  EXPECT_CALL(
      *payments_network_interface_,
      GetDetailsForCreatePaymentInstrument(0, client_token, _, "en-US"));

  test_api(*manager_).DoOnClientTokenReceived(client_token);
}

}  // namespace
}  // namespace payments::facilitated
