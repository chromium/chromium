// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_event_response_util.h"

#include <cstdint>
#include <set>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"

namespace payments {
namespace {

TEST(PaymentEventResponseUtilTest, CanMakePaymentEventResponse) {
  std::set<mojom::CanMakePaymentEventResponseType> no_error = {
      mojom::CanMakePaymentEventResponseType::INCOGNITO,
      mojom::CanMakePaymentEventResponseType::SUCCESS};
  for (const auto& response_type : no_error) {
    EXPECT_TRUE(
        ConvertCanMakePaymentEventResponseTypeToErrorString(response_type)
            .empty());
  }

  constexpr int32_t kMin =
      static_cast<int32_t>(mojom::CanMakePaymentEventResponseType::kMinValue);
  constexpr int32_t kMax =
      static_cast<int32_t>(mojom::CanMakePaymentEventResponseType::kMaxValue);
  for (int32_t i = kMin; i < kMax; ++i) {
    mojom::CanMakePaymentEventResponseType response_type =
        static_cast<mojom::CanMakePaymentEventResponseType>(i);
    if (no_error.find(response_type) != no_error.end())
      continue;

    std::string_view error_string =
        ConvertCanMakePaymentEventResponseTypeToErrorString(response_type);
    EXPECT_LT(2U, error_string.length());
    EXPECT_EQ('.', error_string[error_string.length() - 1]);
  }
}

TEST(PaymentEventResponseUtilTest, PaymentRequestEventResponse) {
  EXPECT_TRUE(ConvertPaymentEventResponseTypeToErrorString(
                  mojom::PaymentEventResponseType::PAYMENT_EVENT_SUCCESS)
                  .empty());

  constexpr int32_t kMin =
      static_cast<int32_t>(mojom::PaymentEventResponseType::kMinValue);
  constexpr int32_t kMax =
      static_cast<int32_t>(mojom::PaymentEventResponseType::kMaxValue);
  for (int32_t i = kMin; i < kMax; ++i) {
    mojom::PaymentEventResponseType response_type =
        static_cast<mojom::PaymentEventResponseType>(i);
    if (mojom::PaymentEventResponseType::PAYMENT_EVENT_SUCCESS == response_type)
      continue;

    std::string_view error_string =
        ConvertPaymentEventResponseTypeToErrorString(response_type);
    EXPECT_LT(2U, error_string.length());
    EXPECT_EQ('.', error_string[error_string.length() - 1]);
  }
}

}  // namespace
}  // namespace payments
