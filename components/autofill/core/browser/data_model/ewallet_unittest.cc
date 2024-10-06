// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/ewallet.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(EwalletTest, VerifyAllFields) {
  Ewallet ewallet(100, u"nickname", GURL("http://www.example.com"),
                  u"ewallet_name", u"account_display_name",
                  {u"supported_payment_link_uri_1"}, /*is_fido_enrolled=*/true);

  EXPECT_EQ(100, ewallet.payment_instrument().instrument_id());
  EXPECT_EQ(u"ewallet_name", ewallet.ewallet_name());
  EXPECT_EQ(u"account_display_name", ewallet.account_display_name());
  EXPECT_EQ(u"nickname", ewallet.payment_instrument().nickname());
  EXPECT_TRUE(ewallet.supported_payment_link_uris().contains(
      u"supported_payment_link_uri_1"));
  EXPECT_EQ(GURL("http://www.example.com"),
            ewallet.payment_instrument().display_icon_url());
  EXPECT_TRUE(ewallet.payment_instrument().IsSupported(
      PaymentInstrument::PaymentRail::kPaymentHyperlink));
  EXPECT_TRUE(ewallet.payment_instrument().is_fido_enrolled());
}

TEST(EwalletTest, VerifyIsFidoEnrolled_FidoNotEnrolled) {
  Ewallet ewallet(100, u"nickname", GURL("http://www.example.com"),
                  u"ewallet_name", u"account_display_name",
                  {u"supported_payment_link_uri_1"},
                  /*is_fido_enrolled=*/false);

  EXPECT_FALSE(ewallet.payment_instrument().is_fido_enrolled());
}

TEST(EwalletTest, PaymentInstrumentInstrumentIdSmaller) {
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  Ewallet ewallet_2(200, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  // ewallet_1 is smaller than ewallet_2 because 100 < 200.
  EXPECT_TRUE(ewallet_1 < ewallet_2);
}

TEST(EwalletTest, PaymentInstrumentInstrumentIdBigger) {
  Ewallet ewallet_1(200, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  // ewallet_1 is bigger than ewallet_2 because 200 > 100.
  EXPECT_TRUE(ewallet_1 > ewallet_2);
}

TEST(EwalletTest, PaymentInstrumentNickNameSmaller) {
  std::u16string nick_name_1 = u"nick_name_1";
  Ewallet ewallet_1(100, nick_name_1, GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);
  std::u16string nick_name_2 = u"nick_name_2";
  Ewallet ewallet_2(100, nick_name_2, GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  // ewallet_1 is smaller than ewallet_2 because nick_name_1 < nick_name_2.
  EXPECT_TRUE(ewallet_1 < ewallet_2);
}

TEST(EwalletTest, PaymentInstrumentNickNameBigger) {
  std::u16string nick_name_1 = u"nick_name_2";
  Ewallet ewallet_1(100, nick_name_1, GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);
  std::u16string nick_name_2 = u"nick_name_1";
  Ewallet ewallet_2(100, nick_name_2, GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  // ewallet_1 is bigger than ewallet_2 because nick_name_1 > nick_name_2.
  EXPECT_TRUE(ewallet_1 > ewallet_2);
}

TEST(EwalletTest, EwalletNameSmaller) {
  std::u16string ewallet_name_1 = u"ewallet_name_1";
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    ewallet_name_1, u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);
  std::u16string ewallet_name_2 = u"ewallet_name_2";
  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    ewallet_name_2, u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  // ewallet_1 is smaller than ewallet_2 because ewallet_name_1 <
  // ewallet_name_2.
  EXPECT_TRUE(ewallet_1 < ewallet_2);
}

TEST(EwalletTest, EwalletNameBigger) {
  std::u16string ewallet_name_1 = u"ewallet_name_2";
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    ewallet_name_1, u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);
  std::u16string ewallet_name_2 = u"ewallet_name_1";
  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    ewallet_name_2, u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  // ewallet_1 is bigger than ewallet_2 because ewallet_name_1 > ewallet_name_2.
  EXPECT_TRUE(ewallet_1 > ewallet_2);
}

TEST(EwalletTest, AccountDisplayNameSmaller) {
  std::u16string account_display_name_1 = u"account_display_name_1";
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", account_display_name_1,
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  std::u16string account_display_name_2 = u"account_display_name_2";
  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", account_display_name_2,
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  // ewallet_1 is smaller than ewallet_2 because account_display_name_1 <
  // account_display_name_2.
  EXPECT_TRUE(ewallet_1 < ewallet_2);
}

TEST(EwalletTest, AccountDisplayNameGreater) {
  std::u16string account_display_name_1 = u"account_display_name_2";
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", account_display_name_1,
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  std::u16string account_display_name_2 = u"account_display_name_1";
  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", account_display_name_2,
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  // ewallet_1 is bigger than ewallet_2 because account_display_name_1 >
  // account_display_name_2.
  EXPECT_TRUE(ewallet_1 > ewallet_2);
}

TEST(EwalletTest, SupportedPaymentLinkUrisSmaller) {
  base::flat_set<std::u16string> supported_payment_link_uris_1 = {
      u"supported_payment_link_uris_1"};
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    supported_payment_link_uris_1, /*is_fido_enrolled=*/true);

  base::flat_set<std::u16string> supported_payment_link_uris_2 = {
      u"supported_payment_link_uris_2"};
  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    supported_payment_link_uris_2, /*is_fido_enrolled=*/true);

  // ewallet_1 is smaller than ewallet_2 because supported_payment_link_uris_1 <
  // supported_payment_link_uris_2.
  EXPECT_TRUE(ewallet_1 < ewallet_2);
}

TEST(EwalletTest, SupportedPaymentLinkUrisGreater) {
  base::flat_set<std::u16string> supported_payment_link_uris_1 = {
      u"supported_payment_link_uris_2"};
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    supported_payment_link_uris_1, /*is_fido_enrolled=*/true);

  base::flat_set<std::u16string> supported_payment_link_uris_2 = {
      u"supported_payment_link_uris_1"};
  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    supported_payment_link_uris_2, /*is_fido_enrolled=*/true);

  // ewallet_1 is bigger than ewallet_2 because supported_payment_link_uris_1 >
  // supported_payment_link_uris_2.
  EXPECT_TRUE(ewallet_1 > ewallet_2);
}

TEST(EwalletTest, PaymentInstrumentIsFidoEnrolledSmaller) {
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/false);
  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  // ewallet_1 is smaller than ewallet_2 because is_fido_enrolled <
  // is_fido_enrolled.
  EXPECT_TRUE(ewallet_1 < ewallet_2);
}

TEST(EwalletTest, PaymentInstrumentIsFidoEnrolledBigger) {
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);
  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/false);

  // ewallet_1 is bigger than ewallet_2 because is_fido_enrolled >
  // is_fido_enrolled.
  EXPECT_TRUE(ewallet_1 > ewallet_2);
}

TEST(EwalletTest, IdenticalEwallets) {
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  Ewallet ewallet_2(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  EXPECT_TRUE(ewallet_1 == ewallet_2);
}

TEST(EwalletTest, DifferentEwallets) {
  Ewallet ewallet_1(100, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  Ewallet ewallet_2(200, u"nickname", GURL("http://www.example.com"),
                    u"ewallet_name", u"account_display_name",
                    {u"supported_payment_link_uri_1"},
                    /*is_fido_enrolled=*/true);

  EXPECT_FALSE(ewallet_1 == ewallet_2);
}

}  // namespace autofill
