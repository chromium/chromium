// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/has_enrolled_instrument_query.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

class HasEnrolledInstrumentQueryTest : public ::testing::Test {
 protected:
  HasEnrolledInstrumentQuery guard_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// An HTTPS website is not allowed to query all of the networks of the cards in
// user's autofill database.
TEST_F(HasEnrolledInstrumentQueryTest,
       SameHttpsOriginCannotQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// A localhost website is not allowed to query all of the networks of the cards
// in user's autofill database.
TEST_F(HasEnrolledInstrumentQueryTest,
       SameLocalhostOriginCannotQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("http://localhost:8080"), GURL("http://localhost:8080"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("http://localhost:8080"), GURL("http://localhost:8080"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// A file website is not allowed to query all of the networks of the cards in
// user's autofill database.
TEST_F(HasEnrolledInstrumentQueryTest,
       SameFileOriginCannotQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("file:///tmp/test.html"), GURL("file:///tmp/test.html"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("file:///tmp/test.html"), GURL("file:///tmp/test.html"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// Different HTTPS websites are allowed to query different card networks in
// user's autofill database.
TEST_F(HasEnrolledInstrumentQueryTest,
       DifferentHttpsOriginsCanQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}}));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://not-example.test"), GURL("https://not-example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// Different localhost websites are allowed to query different card networks in
// user's autofill database.
TEST_F(HasEnrolledInstrumentQueryTest,
       DifferentLocalhostOriginsCanQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("http://localhost:8080"), GURL("http://localhost:8080"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}}));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("http://localhost:9090"), GURL("http://localhost:9090"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// Different file websites are allowed to query different card networks in
// user's autofill database.
TEST_F(HasEnrolledInstrumentQueryTest,
       DifferentFileOriginsCanQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("file:///tmp/test.html"), GURL("file:///tmp/test.html"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}}));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("file:///tmp/not-test.html"), GURL("file:///tmp/not-test.html"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// The same website is not allowed to query the same payment method with
// different parameters.
TEST_F(HasEnrolledInstrumentQueryTest,
       SameOriginCannotQueryBasicCardWithTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
       {"https://alicepay.test", {"{alicePayParameter: 1}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
       {"https://bobpay.test", {"{bobPayParameter: 2}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}},
       {"https://bobpay.test", {"{bobPayParameter: 2}"}}}));
}

// Two different websites are allowed to query the same payment method with
// different parameters.
TEST_F(HasEnrolledInstrumentQueryTest,
       DifferentOriginsCanQueryBasicCardWithTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}}}));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://not-example.test"), GURL("https://not-example.test"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}}));
}

// A website can query several different payment methods, as long as each
// payment method is queried with the same payment-method-specific data.
TEST_F(HasEnrolledInstrumentQueryTest,
       SameOriginCanQuerySeveralDifferentPaymentMethodIdentifiers) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
       {"https://alicepay.test", {"{alicePayParameter: 1}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://alicepay.test", {"{alicePayParameter: 1}"}},
       {"https://bobpay.test", {"{bobPayParameter: 2}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://bobpay.test", {"{bobPayParameter: 2}"}},
       {"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// A website cannot query several different payment methods without the
// per-method quota, even if method-specific data remains unchanged.
TEST_F(HasEnrolledInstrumentQueryTest,
       SameOriginCannotQueryDifferentMethodsWithoutPerMethodQuota) {
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
       {"https://alicepay.test", {"{alicePayParameter: 1}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://alicepay.test", {"{alicePayParameter: 1}"}},
       {"https://bobpay.test", {"{bobPayParameter: 2}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://bobpay.test", {"{bobPayParameter: 2}"}},
       {"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// An instance of a website with per-method quota enabled (e.g., through an
// origin trial) can query different payment methods, as long as each payment
// method is queried with the same method-specific data. Another instance of the
// same website (e.g., in a different tab) without the per-method quota feature
// cannot query different payment methods.
TEST_F(HasEnrolledInstrumentQueryTest, SameWebsiteDifferentQuotaPolicy) {
  // First instance of https://example.test has per-method quota feature enabled
  // and so can query different payment methods, as long as the method-specific
  // data stays the same.
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
       {"https://alicepay.test", {"{alicePayParameter: 1}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://alicepay.test", {"{alicePayParameter: 1}"}},
       {"https://bobpay.test", {"{bobPayParameter: 2}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://bobpay.test", {"{bobPayParameter: 2}"}},
       {"basic-card", {"{supportedNetworks: ['visa']}"}}}));

  // Second instance of https://example.test has per-method quota feature
  // disabled and so can only repeat the first query.
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://alicepay.test", {"{alicePayParameter: 1}"}},
       {"https://bobpay.test", {"{bobPayParameter: 2}"}}}));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
       {"https://alicepay.test", {"{alicePayParameter: 1}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://bobpay.test", {"{bobPayParameter: 2}"}},
       {"basic-card", {"{supportedNetworks: ['visa']}"}}}));

  // The two website queries can be interleaved any number of times in any order
  // with the same results.
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
       {"https://alicepay.test", {"{alicePayParameter: 1}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://bobpay.test", {"{bobPayParameter: 2}"}},
       {"basic-card", {"{supportedNetworks: ['visa']}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://alicepay.test", {"{alicePayParameter: 1}"}},
       {"https://bobpay.test", {"{bobPayParameter: 2}"}}}));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
       {"https://alicepay.test", {"{alicePayParameter: 1}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://alicepay.test", {"{alicePayParameter: 1}"}},
       {"https://bobpay.test", {"{bobPayParameter: 2}"}}}));
  EXPECT_FALSE(guard_.CanQuery(
      GURL("https://example.test"), GURL("https://example.test"),
      {{"https://bobpay.test", {"{bobPayParameter: 2}"}},
       {"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

}  // namespace
}  // namespace payments
