// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/can_make_payment_query.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

class CanMakePaymentQueryTest : public ::testing::Test {
 protected:
  CanMakePaymentQuery guard_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// An HTTPS website is not allowed to query all of the networks of the cards in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       SameHttpsOriginCannotQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['amex']}"}}},
                      /*per_method_quota=*/true));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}}},
                      /*per_method_quota=*/true));
}

// A localhost website is not allowed to query all of the networks of the cards
// in user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       SameLocalhostOriginCannotQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("http://localhost:8080"), GURL("http://localhost:8080"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}},
      /*per_method_quota=*/true));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("http://localhost:8080"), GURL("http://localhost:8080"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}},
      /*per_method_quota=*/true));
}

// A file website is not allowed to query all of the networks of the cards in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       SameFileOriginCannotQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("file:///tmp/test.html"), GURL("file:///tmp/test.html"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}},
      /*per_method_quota=*/true));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("file:///tmp/test.html"), GURL("file:///tmp/test.html"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}},
      /*per_method_quota=*/true));
}

// Different HTTPS websites are allowed to query different card networks in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       DifferentHttpsOriginsCanQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['amex']}"}}},
                      /*per_method_quota=*/true));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://not-example.com"), GURL("https://not-example.com"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}},
      /*per_method_quota=*/true));
}

// Different localhost websites are allowed to query different card networks in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       DifferentLocalhostOriginsCanQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("http://localhost:8080"), GURL("http://localhost:8080"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}},
      /*per_method_quota=*/true));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("http://localhost:9090"), GURL("http://localhost:9090"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}},
      /*per_method_quota=*/true));
}

// Different file websites are allowed to query different card networks in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       DifferentFileOriginsCanQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("file:///tmp/test.html"), GURL("file:///tmp/test.html"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}},
      /*per_method_quota=*/true));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("file:///tmp/not-test.html"), GURL("file:///tmp/not-test.html"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}},
      /*per_method_quota=*/true));
}

// The same website is not allowed to query the same payment method with
// different parameters.
TEST_F(CanMakePaymentQueryTest,
       SameOriginCannotQueryBasicCardWithTwoDifferentCardNetworks) {
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}},
                      /*per_method_quota=*/true));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}},
                      /*per_method_quota=*/true));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['amex']}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}},
                      /*per_method_quota=*/true));
}

// Two different websites are allowed to query the same payment method with
// different parameters.
TEST_F(CanMakePaymentQueryTest,
       DifferentOriginsCanQueryBasicCardWithTwoDifferentCardNetworks) {
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}}},
                      /*per_method_quota=*/true));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://not-example.com"), GURL("https://not-example.com"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}},
      /*per_method_quota=*/true));
}

// A website can query several different payment methods, as long as each
// payment method is queried with the same payment-method-specific data.
TEST_F(CanMakePaymentQueryTest,
       SameOriginCanQuerySeveralDifferentPaymentMethodIdentifiers) {
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}},
                      /*per_method_quota=*/true));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://alicepay.com", {"{alicePayParameter: 1}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}},
                      /*per_method_quota=*/true));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://bobpay.com", {"{bobPayParameter: 2}"}},
                       {"basic-card", {"{supportedNetworks: ['visa']}"}}},
                      /*per_method_quota=*/true));
}

// A website cannot query several different payment methods without the
// per-method quota, even if method-specific data remains unchanged.
TEST_F(CanMakePaymentQueryTest,
       SameOriginCannotQueryDifferentMethodsWithoutPerMethodQuota) {
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}},
                      /*per_method_quota=*/false));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://alicepay.com", {"{alicePayParameter: 1}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}},
                      /*per_method_quota=*/false));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://bobpay.com", {"{bobPayParameter: 2}"}},
                       {"basic-card", {"{supportedNetworks: ['visa']}"}}},
                      /*per_method_quota=*/false));
}

// An instance of a website with per-method quota enabled (e.g., through an
// origin trial) can query different payment methods, as long as each payment
// method is queried with the same method-specific data. Another instance of the
// same website (e.g., in a different tab) without the per-method quota feature
// cannot query different payment methods.
TEST_F(CanMakePaymentQueryTest, SameWebsiteDifferentQuotaPolicy) {
  // First instance of https://example.com has per-method quota feature enabled
  // and so can query different payment methods, as long as the method-specific
  // data stays the same.
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}},
                      /*per_method_quota=*/true));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://alicepay.com", {"{alicePayParameter: 1}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}},
                      /*per_method_quota=*/true));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://bobpay.com", {"{bobPayParameter: 2}"}},
                       {"basic-card", {"{supportedNetworks: ['visa']}"}}},
                      /*per_method_quota=*/true));

  // Second instance of https://example.com has per-method quota feature
  // disabled and so can only repeat the first query.
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://alicepay.com", {"{alicePayParameter: 1}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}},
                      /*per_method_quota=*/false));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}},
                      /*per_method_quota=*/false));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://bobpay.com", {"{bobPayParameter: 2}"}},
                       {"basic-card", {"{supportedNetworks: ['visa']}"}}},
                      /*per_method_quota=*/false));

  // The two website queries can be interleaved any number of times in any order
  // with the same results.
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}},
                      /*per_method_quota=*/true));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://bobpay.com", {"{bobPayParameter: 2}"}},
                       {"basic-card", {"{supportedNetworks: ['visa']}"}}},
                      /*per_method_quota=*/true));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://alicepay.com", {"{alicePayParameter: 1}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}},
                      /*per_method_quota=*/false));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}},
                      /*per_method_quota=*/false));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://alicepay.com", {"{alicePayParameter: 1}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}},
                      /*per_method_quota=*/true));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://bobpay.com", {"{bobPayParameter: 2}"}},
                       {"basic-card", {"{supportedNetworks: ['visa']}"}}},
                      /*per_method_quota=*/false));
}

}  // namespace
}  // namespace payments
