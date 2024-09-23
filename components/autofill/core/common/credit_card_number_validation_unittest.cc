// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/credit_card_number_validation.h"

#include <cstddef>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// From
// https://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm
constexpr auto kValidNumbers = std::to_array<std::u16string_view>(
    {u"378282246310005",     u"3714 4963 5398 431",  u"3787-3449-3671-000",
     u"5610591081018250",    u"3056 9309 0259 04",   u"3852-0000-0232-37",
     u"6011111111111117",    u"6011 0009 9013 9424", u"3530-1113-3330-0000",
     u"3566002020360505",
     u"5555 5555 5555 4444",  // Mastercard.
     u"5105-1051-0510-5100",
     u"4111111111111111",  // Visa.
     u"4012 8888 8888 1881", u"4222-2222-2222-2",    u"5019717010103742",
     u"6331101999990016",    u"6247130048162403",
     u"4532261615476013542",  // Visa, 19 digits.
     u"5067071446391278",     // Elo.
     u"5060995764815772",     // Verve.
     u"506099576481577267",   // Verve 18 digits.
     u"5060995764815772675",  // Verve 19 digits.
     u"6362970000457013"});
constexpr auto kInvalidNumbers = std::to_array<std::u16string_view>({
    u"4111 1111 112",        /* too short */
    u"41111111111111111115", /* too long */
    u"4111-1111-1111-1110",  /* wrong Luhn checksum */
    u"3056 9309 0259 04aa",  /* non-digit characters */
    u"50609957648157726"     /* Verve 17 digits */
});

TEST(AutofillValidation, IsValidCreditCardNumber) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableVerveCardSupport};

  for (std::u16string_view valid_number : kValidNumbers) {
    SCOPED_TRACE(base::UTF16ToUTF8(valid_number));
    EXPECT_TRUE(IsValidCreditCardNumber(valid_number));
  }
  for (std::u16string_view invalid_number : kInvalidNumbers) {
    SCOPED_TRACE(base::UTF16ToUTF8(invalid_number));
    EXPECT_FALSE(IsValidCreditCardNumber(invalid_number));
  }
}

// Tests the plausibility of the length of the supplied credit card number.
TEST(AutofillValidation, IsValidCreditCardNumberLength) {
  for (std::u16string_view valid_number : kValidNumbers) {
    SCOPED_TRACE(base::UTF16ToUTF8(valid_number));
    EXPECT_TRUE(HasCorrectCreditCardNumberLength(
        StripCardNumberSeparators(valid_number)));
  }
  // Only the first 2 invalid numbers in kInvalidNumbers have a bad length.
  static_assert(2 <= kInvalidNumbers.size());
  for (size_t i = 0; i < 2; ++i) {
    std::u16string_view invalid_number = kInvalidNumbers[i];
    SCOPED_TRACE(base::UTF16ToUTF8(invalid_number));
    EXPECT_FALSE(HasCorrectCreditCardNumberLength(
        StripCardNumberSeparators(invalid_number)));
  }
}

// Tests the validation of credit card numbers using the Luhn check.
TEST(AutofillValidation, CreditCardNumberLuhnTest) {
  for (std::u16string_view valid_number : kValidNumbers) {
    SCOPED_TRACE(base::UTF16ToUTF8(valid_number));
    EXPECT_TRUE(PassesLuhnCheck(StripCardNumberSeparators(valid_number)));
  }

  constexpr std::u16string_view invalid_luhn_number = kInvalidNumbers[2];
  SCOPED_TRACE(base::UTF16ToUTF8(invalid_luhn_number));
  EXPECT_FALSE(PassesLuhnCheck(invalid_luhn_number));
}

struct GetCardNetworkTestCase {
  const char* card_number;
  const char* issuer_network;
  bool is_valid;
};

// We are doing batches here because INSTANTIATE_TEST_SUITE_P has a
// 50 upper limit.
class GetCardNetworkTestBatch1
    : public testing::TestWithParam<GetCardNetworkTestCase> {};

TEST_P(GetCardNetworkTestBatch1, GetCardNetwork) {
  auto test_case = GetParam();
  std::u16string card_number = base::ASCIIToUTF16(test_case.card_number);
  SCOPED_TRACE(card_number);
  EXPECT_EQ(test_case.issuer_network, GetCardNetwork(card_number));
  EXPECT_EQ(test_case.is_valid, IsValidCreditCardNumber(card_number));
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    GetCardNetworkTestBatch1,
    testing::Values(
        // The relevant sample numbers from
        // http://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm
        GetCardNetworkTestCase{"378282246310005", kAmericanExpressCard, true},
        GetCardNetworkTestCase{"371449635398431", kAmericanExpressCard, true},
        GetCardNetworkTestCase{"378734493671000", kAmericanExpressCard, true},
        GetCardNetworkTestCase{"30569309025904", kDinersCard, true},
        GetCardNetworkTestCase{"38520000023237", kDinersCard, true},
        GetCardNetworkTestCase{"6011111111111117", kDiscoverCard, true},
        GetCardNetworkTestCase{"6011000990139424", kDiscoverCard, true},
        GetCardNetworkTestCase{"3530111333300000", kJCBCard, true},
        GetCardNetworkTestCase{"3566002020360505", kJCBCard, true},
        GetCardNetworkTestCase{"5555555555554444", kMasterCard, true},
        GetCardNetworkTestCase{"5105105105105100", kMasterCard, true},
        GetCardNetworkTestCase{"4111111111111111", kVisaCard, true},
        GetCardNetworkTestCase{"4012888888881881", kVisaCard, true},
        GetCardNetworkTestCase{"4222222222222", kVisaCard, true},
        GetCardNetworkTestCase{"4532261615476013542", kVisaCard, true},

        // The relevant sample numbers from
        // https://www.auricsystems.com/sample-credit-card-numbers/
        GetCardNetworkTestCase{"343434343434343", kAmericanExpressCard, true},
        GetCardNetworkTestCase{"371144371144376", kAmericanExpressCard, true},
        GetCardNetworkTestCase{"341134113411347", kAmericanExpressCard, true},
        GetCardNetworkTestCase{"36438936438936", kDinersCard, true},
        GetCardNetworkTestCase{"36110361103612", kDinersCard, true},
        GetCardNetworkTestCase{"36111111111111", kDinersCard, true},
        GetCardNetworkTestCase{"6011016011016011", kDiscoverCard, true},
        GetCardNetworkTestCase{"6011000990139424", kDiscoverCard, true},
        GetCardNetworkTestCase{"6011000000000004", kDiscoverCard, true},
        GetCardNetworkTestCase{"6011000995500000", kDiscoverCard, true},
        GetCardNetworkTestCase{"6500000000000002", kDiscoverCard, true},
        GetCardNetworkTestCase{"3566002020360505", kJCBCard, true},
        GetCardNetworkTestCase{"3528000000000007", kJCBCard, true},
        GetCardNetworkTestCase{"2222400061240016", kMasterCard, true},
        GetCardNetworkTestCase{"2223000048400011", kMasterCard, true},
        GetCardNetworkTestCase{"5500005555555559", kMasterCard, true},
        GetCardNetworkTestCase{"5555555555555557", kMasterCard, true},
        GetCardNetworkTestCase{"5454545454545454", kMasterCard, true},
        GetCardNetworkTestCase{"5478050000000007", kMasterCard, true},
        GetCardNetworkTestCase{"5112345112345114", kMasterCard, true},
        GetCardNetworkTestCase{"5115915115915118", kMasterCard, true},
        GetCardNetworkTestCase{"6247130048162403", kUnionPay, true},
        GetCardNetworkTestCase{"6247130048162403", kUnionPay, true},
        GetCardNetworkTestCase{"622384452162063648", kUnionPay, true},
        GetCardNetworkTestCase{"2204883716636153", kMirCard, true},
        GetCardNetworkTestCase{"2200111234567898", kMirCard, true},
        GetCardNetworkTestCase{"2200481349288130", kMirCard, true}));

class GetCardNetworkTestBatch2
    : public testing::TestWithParam<GetCardNetworkTestCase> {};

TEST_P(GetCardNetworkTestBatch2, GetCardNetwork) {
  auto test_case = GetParam();
  std::u16string card_number = base::ASCIIToUTF16(test_case.card_number);
  SCOPED_TRACE(card_number);
  EXPECT_EQ(test_case.issuer_network, GetCardNetwork(card_number));
  EXPECT_EQ(test_case.is_valid, IsValidCreditCardNumber(card_number));
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    GetCardNetworkTestBatch2,
    testing::Values(
        // The relevant numbers are sampled from
        // https://www.bincodes.com/bank-creditcard-generator/ and
        // https://www.ebanx.com/business/en/developers/integrations/testing/credit-card-test-numbers
        // It's then modified to fit the correct pattern based on the Elo regex,
        // sourced from the Elo documentation.
        GetCardNetworkTestCase{"5067071446391278", kEloCard, true},
        GetCardNetworkTestCase{"6277800000457016", kEloCard, true},

        // These sample numbers were created by taking the expected card prefix,
        // filling out the required number of digits, and editing the last digit
        // so that the full number passes a Luhn check.
        GetCardNetworkTestCase{"4312741111111112", kEloCard, true},
        GetCardNetworkTestCase{"4514161111111119", kEloCard, true},
        GetCardNetworkTestCase{"5090141111111110", kEloCard, true},
        GetCardNetworkTestCase{"6277801111111112", kEloCard, true},
        GetCardNetworkTestCase{"2205111111111112", kTroyCard, true},
        GetCardNetworkTestCase{"9792111111111116", kTroyCard, true},

        // Existence of separators should not change the result, especially for
        // prefixes that go past the first separator.
        GetCardNetworkTestCase{"4111 1111 1111 1111", kVisaCard, true},
        GetCardNetworkTestCase{"4111-1111-1111-1111", kVisaCard, true},
        GetCardNetworkTestCase{"4312 7411 1111 1112", kEloCard, true},
        GetCardNetworkTestCase{"4312-7411-1111-1112", kEloCard, true},
        GetCardNetworkTestCase{"2205 1111 1111 1112", kTroyCard, true},
        GetCardNetworkTestCase{"2205-1111-1111-1112", kTroyCard, true},

        // Empty string
        GetCardNetworkTestCase{"", kGenericCard, false},

        // Non-numeric
        GetCardNetworkTestCase{"garbage", kGenericCard, false},
        GetCardNetworkTestCase{"4garbage", kGenericCard, false},

        // Fails Luhn check.
        GetCardNetworkTestCase{"4111111111111112", kVisaCard, false},
        GetCardNetworkTestCase{"6247130048162413", kUnionPay, false},
        GetCardNetworkTestCase{"2204883716636154", kMirCard, false},

        // Invalid length.
        GetCardNetworkTestCase{"3434343434343434", kAmericanExpressCard, false},
        GetCardNetworkTestCase{"411111111111116", kVisaCard, false},
        GetCardNetworkTestCase{"220011123456783", kMirCard, false}));

class GetCardNetworkTestBatch3
    : public testing::TestWithParam<GetCardNetworkTestCase> {};

TEST_P(GetCardNetworkTestBatch3, GetCardNetwork) {
  auto test_case = GetParam();
  std::u16string card_number = base::ASCIIToUTF16(test_case.card_number);
  SCOPED_TRACE(card_number);
  EXPECT_EQ(test_case.issuer_network, GetCardNetwork(card_number));
  EXPECT_EQ(test_case.is_valid, IsValidCreditCardNumber(card_number));
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    GetCardNetworkTestBatch3,
    testing::Values(
        // Issuer Identification Numbers (IINs) that Chrome recognizes.
        GetCardNetworkTestCase{"2200", kMirCard, false},
        GetCardNetworkTestCase{"2201", kMirCard, false},
        GetCardNetworkTestCase{"2202", kMirCard, false},
        GetCardNetworkTestCase{"2203", kMirCard, false},
        GetCardNetworkTestCase{"2204", kMirCard, false},
        GetCardNetworkTestCase{"22050", kTroyCard, false},
        GetCardNetworkTestCase{"22051", kTroyCard, false},
        GetCardNetworkTestCase{"22052", kTroyCard, false},
        GetCardNetworkTestCase{"2221", kMasterCard, false},
        GetCardNetworkTestCase{"2720", kMasterCard, false},
        GetCardNetworkTestCase{"300", kDinersCard, false},
        GetCardNetworkTestCase{"301", kDinersCard, false},
        GetCardNetworkTestCase{"302", kDinersCard, false},
        GetCardNetworkTestCase{"303", kDinersCard, false},
        GetCardNetworkTestCase{"304", kDinersCard, false},
        GetCardNetworkTestCase{"305", kDinersCard, false},
        GetCardNetworkTestCase{"309", kDinersCard, false},
        GetCardNetworkTestCase{"34", kAmericanExpressCard, false},
        GetCardNetworkTestCase{"3528", kJCBCard, false},
        GetCardNetworkTestCase{"3531", kJCBCard, false},
        GetCardNetworkTestCase{"3589", kJCBCard, false},
        GetCardNetworkTestCase{"36", kDinersCard, false},
        GetCardNetworkTestCase{"37", kAmericanExpressCard, false},
        GetCardNetworkTestCase{"38", kDinersCard, false},
        GetCardNetworkTestCase{"39", kDinersCard, false},
        GetCardNetworkTestCase{"4", kVisaCard, false},
        GetCardNetworkTestCase{"431274", kEloCard, false},
        GetCardNetworkTestCase{"451416", kEloCard, false},
        GetCardNetworkTestCase{"506707", kEloCard, false},
        GetCardNetworkTestCase{"509014", kEloCard, false},
        GetCardNetworkTestCase{"51", kMasterCard, false},
        GetCardNetworkTestCase{"52", kMasterCard, false},
        GetCardNetworkTestCase{"53", kMasterCard, false},
        GetCardNetworkTestCase{"54", kMasterCard, false},
        GetCardNetworkTestCase{"55", kMasterCard, false},
        GetCardNetworkTestCase{"6011", kDiscoverCard, false},
        GetCardNetworkTestCase{"62", kUnionPay, false},
        GetCardNetworkTestCase{"627780", kEloCard, false},
        GetCardNetworkTestCase{"636368", kEloCard, false},
        GetCardNetworkTestCase{"644", kDiscoverCard, false},
        GetCardNetworkTestCase{"645", kDiscoverCard, false},
        GetCardNetworkTestCase{"646", kDiscoverCard, false},
        GetCardNetworkTestCase{"647", kDiscoverCard, false},
        GetCardNetworkTestCase{"648", kDiscoverCard, false},
        GetCardNetworkTestCase{"649", kDiscoverCard, false},
        GetCardNetworkTestCase{"65", kDiscoverCard, false},
        GetCardNetworkTestCase{"9792", kTroyCard, false}));

class GetCardNetworkTestBatch4
    : public testing::TestWithParam<GetCardNetworkTestCase> {};

TEST_P(GetCardNetworkTestBatch4, GetCardNetwork) {
  auto test_case = GetParam();
  std::u16string card_number = base::ASCIIToUTF16(test_case.card_number);
  SCOPED_TRACE(card_number);
  EXPECT_EQ(test_case.issuer_network, GetCardNetwork(card_number));
  EXPECT_EQ(test_case.is_valid, IsValidCreditCardNumber(card_number));
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    GetCardNetworkTestBatch4,
    testing::Values(
        // Not enough data to determine an IIN uniquely.
        GetCardNetworkTestCase{"2", kGenericCard, false},
        GetCardNetworkTestCase{"3", kGenericCard, false},
        GetCardNetworkTestCase{"30", kGenericCard, false},
        GetCardNetworkTestCase{"35", kGenericCard, false},
        GetCardNetworkTestCase{"5", kGenericCard, false},
        GetCardNetworkTestCase{"6", kGenericCard, false},
        GetCardNetworkTestCase{"60", kGenericCard, false},
        GetCardNetworkTestCase{"601", kGenericCard, false},
        GetCardNetworkTestCase{"64", kGenericCard, false},
        GetCardNetworkTestCase{"9", kGenericCard, false},

        // Unknown IINs.
        GetCardNetworkTestCase{"0", kGenericCard, false},
        GetCardNetworkTestCase{"1", kGenericCard, false},
        GetCardNetworkTestCase{"22053", kGenericCard, false},
        GetCardNetworkTestCase{"306", kGenericCard, false},
        GetCardNetworkTestCase{"307", kGenericCard, false},
        GetCardNetworkTestCase{"308", kGenericCard, false},
        GetCardNetworkTestCase{"31", kGenericCard, false},
        GetCardNetworkTestCase{"32", kGenericCard, false},
        GetCardNetworkTestCase{"33", kGenericCard, false},
        GetCardNetworkTestCase{"351", kGenericCard, false},
        GetCardNetworkTestCase{"3527", kGenericCard, false},
        GetCardNetworkTestCase{"359", kGenericCard, false},
        GetCardNetworkTestCase{"50", kGenericCard, false},
        GetCardNetworkTestCase{"56", kGenericCard, false},
        GetCardNetworkTestCase{"57", kGenericCard, false},
        GetCardNetworkTestCase{"58", kGenericCard, false},
        GetCardNetworkTestCase{"59", kGenericCard, false},
        GetCardNetworkTestCase{"600", kGenericCard, false},
        GetCardNetworkTestCase{"602", kGenericCard, false},
        GetCardNetworkTestCase{"603", kGenericCard, false},
        GetCardNetworkTestCase{"604", kGenericCard, false},
        GetCardNetworkTestCase{"605", kGenericCard, false},
        GetCardNetworkTestCase{"606", kGenericCard, false},
        GetCardNetworkTestCase{"607", kGenericCard, false},
        GetCardNetworkTestCase{"608", kGenericCard, false},
        GetCardNetworkTestCase{"609", kGenericCard, false},
        GetCardNetworkTestCase{"61", kGenericCard, false},
        GetCardNetworkTestCase{"63", kGenericCard, false},
        GetCardNetworkTestCase{"640", kGenericCard, false},
        GetCardNetworkTestCase{"641", kGenericCard, false},
        GetCardNetworkTestCase{"642", kGenericCard, false},
        GetCardNetworkTestCase{"643", kGenericCard, false},
        GetCardNetworkTestCase{"66", kGenericCard, false},
        GetCardNetworkTestCase{"67", kGenericCard, false},
        GetCardNetworkTestCase{"68", kGenericCard, false},
        GetCardNetworkTestCase{"69", kGenericCard, false},
        GetCardNetworkTestCase{"7", kGenericCard, false},
        GetCardNetworkTestCase{"8", kGenericCard, false},
        GetCardNetworkTestCase{"90", kGenericCard, false},
        GetCardNetworkTestCase{"91", kGenericCard, false},
        GetCardNetworkTestCase{"92", kGenericCard, false},
        GetCardNetworkTestCase{"93", kGenericCard, false},
        GetCardNetworkTestCase{"94", kGenericCard, false},
        GetCardNetworkTestCase{"95", kGenericCard, false},
        GetCardNetworkTestCase{"97", kGenericCard, false},
        GetCardNetworkTestCase{"979", kGenericCard, false},
        GetCardNetworkTestCase{"98", kGenericCard, false},
        GetCardNetworkTestCase{"99", kGenericCard, false},

        // Oddball case: Unknown issuer, but valid Luhn check and plausible
        // length.
        GetCardNetworkTestCase{"7000700070007000", kGenericCard, true}));

class GetCardNetworkTestBatch5
    : public testing::TestWithParam<GetCardNetworkTestCase> {};

TEST_P(GetCardNetworkTestBatch5, GetCardNetwork) {
  auto test_case = GetParam();
  std::u16string card_number = base::ASCIIToUTF16(test_case.card_number);
  SCOPED_TRACE(card_number);
  EXPECT_EQ(test_case.issuer_network, GetCardNetwork(card_number));
  EXPECT_EQ(test_case.is_valid, IsValidCreditCardNumber(card_number));
}

// These are the cards that would be wrongly displayed as Discover before Elo
// regex pattern was introduced to match the Elo BIN.
INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    GetCardNetworkTestBatch5,
    testing::Values(GetCardNetworkTestCase{"6500311446391271", kEloCard, true},
                    GetCardNetworkTestCase{"6500401446391270", kEloCard, true},
                    GetCardNetworkTestCase{"6500691446391276", kEloCard, true},
                    GetCardNetworkTestCase{"6500701446391273", kEloCard, true},
                    GetCardNetworkTestCase{"6504061446391278", kEloCard, true},
                    GetCardNetworkTestCase{"6504101446391272", kEloCard, true},
                    GetCardNetworkTestCase{"6504221446391278", kEloCard, true},
                    GetCardNetworkTestCase{"6504301446391278", kEloCard, true},
                    GetCardNetworkTestCase{"6505804463912719", kEloCard, true},
                    GetCardNetworkTestCase{"6504901446391275", kEloCard, true},
                    GetCardNetworkTestCase{"6505001446391273", kEloCard, true},
                    GetCardNetworkTestCase{"6505101446391271", kEloCard, true},
                    GetCardNetworkTestCase{"6505201446391279", kEloCard, true},
                    GetCardNetworkTestCase{"6505301446391277", kEloCard, true},
                    GetCardNetworkTestCase{"6505521446391270", kEloCard, true},
                    GetCardNetworkTestCase{"6505601446391270", kEloCard, true},
                    GetCardNetworkTestCase{"6505701446391278", kEloCard, true},
                    GetCardNetworkTestCase{"6505801446391276", kEloCard, true},
                    GetCardNetworkTestCase{"6505901446391274", kEloCard, true},
                    GetCardNetworkTestCase{"6516521446391277", kEloCard, true},
                    GetCardNetworkTestCase{"6516601446391277", kEloCard, true},
                    GetCardNetworkTestCase{"6516701446391275", kEloCard, true},
                    GetCardNetworkTestCase{"6516881446391275", kEloCard, true},
                    GetCardNetworkTestCase{"6550001446391277", kEloCard, true},
                    GetCardNetworkTestCase{"6550121446391273", kEloCard, true},
                    GetCardNetworkTestCase{"6550261446391277", kEloCard, true},
                    GetCardNetworkTestCase{"6550361446391275", kEloCard, true},
                    GetCardNetworkTestCase{"6550511446391275", kEloCard,
                                           true}));

class GetCardNetworkTestForVerve
    : public testing::TestWithParam<std::u16string> {
 public:
  GetCardNetworkTestForVerve() {
    scoped_feature_list.InitAndEnableFeature(
        features::kAutofillEnableVerveCardSupport);
  }

  GetCardNetworkTestForVerve(const GetCardNetworkTestForVerve&) = delete;
  GetCardNetworkTestForVerve& operator=(const GetCardNetworkTestForVerve&) =
      delete;

  base::test::ScopedFeatureList scoped_feature_list;
};

TEST_P(GetCardNetworkTestForVerve, GetCardNetwork) {
  SCOPED_TRACE(GetParam());
  EXPECT_EQ(kVerveCard, GetCardNetwork(GetParam()));
  EXPECT_TRUE(IsValidCreditCardNumber(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    CreditCardTest,
    GetCardNetworkTestForVerve,
    testing::Values(  // These card numbers have been randomly generated within
                      // the defined Verve BIN ranges.
        u"5060995764815772",
        u"5060990806358451",
        u"5060997273266067",
        u"5060997740230613",
        u"5060992737163430",
        u"5061987110470625",
        u"5061987445171377",
        u"5061981765105453",
        u"5061983034587008",
        u"5061980318637566",
        u"5078650770753174",
        u"5078651316662341",
        u"5078654408537421",
        u"5078651087312688",
        u"5078657863711753",
        u"5079644313343253",
        u"5079640116033778",
        u"5079648281607102",
        u"5079648238122783",
        u"5079641364480216",
        u"6500024766120234",
        u"6500026554888641",
        u"6500023336434125",
        u"6500022576606814",
        u"6500025257576727",
        u"6500277710817343",
        u"6500271430718311",
        u"6500273357378327",
        u"6500270580206358",
        u"6500270130817308"));

}  // namespace
}  // namespace autofill
