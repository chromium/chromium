// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/merchant_page_classifier.h"

#include <string_view>

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "components/payments/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

using enum WebPaymentsClassifierResult;
using enum MerchantPageType;

class MerchantPageClassifierTest : public testing::Test {
 protected:
  MerchantPageClassifier CreateClassifier(
      const base::FieldTrialParams& params) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kCheckoutTelemetry, params);
    return MerchantPageClassifier();
  }

  MerchantPageClassifier classifier_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MerchantPageClassifierTest, MatchSourcesAndCaseInsensitivity) {
  EXPECT_EQ(classifier_.Classify(GURL("https://store.example/products/shoes"),
                                 u"Running Shoes"),
            (MerchantClassificationResult{kNone, kNoMatch}));

  EXPECT_EQ(
      classifier_.Classify(GURL("http://store.example/CHECKOUT?step=2"), u""),
      (MerchantClassificationResult{kCheckoutPage, kUrlOnly}));
  EXPECT_EQ(
      classifier_.Classify(GURL("https://store.example/"), u"SECURE CHECKOUT"),
      (MerchantClassificationResult{kCheckoutPage, kTitleOnly}));
  EXPECT_EQ(classifier_.Classify(GURL("https://store.example/checkout"),
                                 u"Secure Checkout"),
            (MerchantClassificationResult{kCheckoutPage, kBothUrlAndTitle}));

  EXPECT_EQ(classifier_.Classify(
                GURL("https://store.example/Order-Confirmation"), u""),
            (MerchantClassificationResult{kOrderConfirmationPage, kUrlOnly}));
  EXPECT_EQ(classifier_.Classify(GURL("https://store.example/"),
                                 u"ORDER CONFIRMATION"),
            (MerchantClassificationResult{kOrderConfirmationPage, kTitleOnly}));
  EXPECT_EQ(
      classifier_.Classify(GURL("https://store.example/order-confirmation"),
                           u"Order Confirmation"),
      (MerchantClassificationResult{kOrderConfirmationPage, kBothUrlAndTitle}));
}

TEST_F(MerchantPageClassifierTest, OrderConfirmationPrecedesCheckout) {
  EXPECT_EQ(classifier_.Classify(
                GURL("https://store.example/checkout/thank-you"), u"Checkout"),
            (MerchantClassificationResult{kOrderConfirmationPage, kUrlOnly}));
  EXPECT_EQ(
      classifier_.Classify(GURL("https://store.example/checkout/thank-you"),
                           u"Thank You - Checkout"),
      (MerchantClassificationResult{kOrderConfirmationPage, kBothUrlAndTitle}));
  EXPECT_EQ(
      classifier_.Classify(GURL("https://store.example/checkout/complete"),
                           u"Order Confirmation"),
      (MerchantClassificationResult{kOrderConfirmationPage, kTitleOnly}));
}

TEST_F(MerchantPageClassifierTest, IgnoresInvalidAndNonHttpUrls) {
  for (std::string_view url : {
           "",
           "https://:invalid_port/checkout",
           "chrome://checkout/payment",
           "about:blank",
           "data:text/html,/checkout",
           "file:///checkout",
       }) {
    EXPECT_EQ(classifier_.Classify(GURL(url), u"Order Confirmation"),
              (MerchantClassificationResult{kNone, kNoMatch}))
        << url;
  }
}

TEST_F(MerchantPageClassifierTest, MatchesFullUrlIncludingQueryAndRef) {
  EXPECT_EQ(
      classifier_.Classify(GURL("http://store.example.com/login"
                                "?and_then=http://store.example.com/checkout"),
                           u""),
      (MerchantClassificationResult{kCheckoutPage, kUrlOnly}));
  EXPECT_EQ(classifier_.Classify(
                GURL("https://store.example/login?next=checkout"), u""),
            (MerchantClassificationResult{kNone, kNoMatch}));
  EXPECT_EQ(
      classifier_.Classify(GURL("https://checkout.store.example/cart"), u""),
      (MerchantClassificationResult{kNone, kNoMatch}));
  EXPECT_EQ(classifier_.Classify(
                GURL("https://checkout:payment@store.example/cart"), u""),
            (MerchantClassificationResult{kNone, kNoMatch}));
  EXPECT_EQ(
      classifier_.Classify(GURL("https://store.example/cart#/checkouts"), u""),
      (MerchantClassificationResult{kCheckoutPage, kUrlOnly}));
  EXPECT_EQ(
      classifier_.Classify(GURL("https://store.example/shop#/thankyou"), u""),
      (MerchantClassificationResult{kOrderConfirmationPage, kUrlOnly}));
  EXPECT_EQ(classifier_.Classify(
                GURL("https://store.example/checkout#shipping"), u""),
            (MerchantClassificationResult{kCheckoutPage, kUrlOnly}));
}

TEST_F(MerchantPageClassifierTest, HandlesEmptyAndMalformedUtf16Titles) {
  EXPECT_EQ(classifier_.Classify(GURL("https://store.example/"),
                                 u"Caf\u00e9 \u00b7 Checkout"),
            (MerchantClassificationResult{kCheckoutPage, kTitleOnly}));
  EXPECT_EQ(classifier_.Classify(GURL("https://store.example/"),
                                 u"\xd800Order Confirmation"),
            (MerchantClassificationResult{kOrderConfirmationPage, kTitleOnly}));

  MerchantPageClassifier classifier =
      CreateClassifier({{"checkout_title_regex", ".*"}});
  EXPECT_EQ(classifier.Classify(GURL("https://store.example/"), u""),
            (MerchantClassificationResult{kNone, kNoMatch}));
  EXPECT_EQ(classifier.Classify(GURL("https://store.example/"), u"x"),
            (MerchantClassificationResult{kCheckoutPage, kTitleOnly}));
}

TEST_F(MerchantPageClassifierTest, OverridesRegexesViaFeatureParams) {
  MerchantPageClassifier classifier = CreateClassifier({
      {"checkout_url_regex", ".*/bag(?:[/?#].*|$)"},
      {"order_confirmation_url_regex", ".*/(?-i:Confirmed)(?:[/?#].*|$)"},
      {"checkout_title_regex", R"(\bpanier\b)"},
      {"order_confirmation_title_regex", "\\bcommande confirm\u00e9e\\b"},
  });

  // Existing `classifier_` keeps the patterns compiled at its construction.
  EXPECT_EQ(
      classifier_.Classify(GURL("https://store.example/checkout"), u"Checkout"),
      (MerchantClassificationResult{kCheckoutPage, kBothUrlAndTitle}));

  EXPECT_EQ(
      classifier.Classify(GURL("https://store.example/checkout"), u"Checkout"),
      (MerchantClassificationResult{kNone, kNoMatch}));
  EXPECT_EQ(classifier.Classify(GURL("https://store.example/bag"), u"Panier"),
            (MerchantClassificationResult{kCheckoutPage, kBothUrlAndTitle}));
  EXPECT_EQ(classifier.Classify(GURL("https://store.example/confirmed"), u""),
            (MerchantClassificationResult{kNone, kNoMatch}));
  EXPECT_EQ(
      classifier.Classify(GURL("https://store.example/Confirmed"),
                          u"Commande Confirm\u00c9e"),
      (MerchantClassificationResult{kOrderConfirmationPage, kBothUrlAndTitle}));
}

TEST_F(MerchantPageClassifierTest, IgnoresInvalidAndEmptyRegexParams) {
  MerchantPageClassifier broken_classifier = CreateClassifier({
      {"checkout_url_regex", "[unclosed"},
      {"order_confirmation_title_regex", "*invalid"},
  });
  EXPECT_EQ(
      broken_classifier.Classify(GURL("https://store.example/checkout"), u""),
      (MerchantClassificationResult{kNone, kNoMatch}));
  EXPECT_EQ(broken_classifier.Classify(GURL("https://store.example/checkout"),
                                       u"Checkout"),
            (MerchantClassificationResult{kCheckoutPage, kTitleOnly}));
  EXPECT_EQ(broken_classifier.Classify(
                GURL("https://store.example/order-confirmation"),
                u"Order Confirmation"),
            (MerchantClassificationResult{kOrderConfirmationPage, kUrlOnly}));

  MerchantPageClassifier empty_param_classifier =
      CreateClassifier({{"checkout_url_regex", ""}});
  EXPECT_EQ(empty_param_classifier.Classify(
                GURL("https://store.example/checkout"), u""),
            (MerchantClassificationResult{kNone, kNoMatch}));
  EXPECT_EQ(empty_param_classifier.Classify(
                GURL("https://store.example/order-confirmation"), u""),
            (MerchantClassificationResult{kOrderConfirmationPage, kUrlOnly}));
}

}  // namespace
}  // namespace payments
