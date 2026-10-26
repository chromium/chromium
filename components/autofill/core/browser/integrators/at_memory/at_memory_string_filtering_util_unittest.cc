// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/at_memory_string_filtering_util.h"

#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Tests exact token matching without edits.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_ExactMatch) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"john", u"john"));
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"street", u"street"));
}

// Tests short query tokens (len <= 2) which require 0 edits.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_ShortTokens) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"john", u"jo"));
  EXPECT_FALSE(FuzzyMatchesSingleToken(u"john", u"ji"));
}

// Tests medium-length query tokens (3 <= len <= 5) which allow up to 1 edit.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_MediumTokens) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"john", u"jhn"));
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"street", u"strt"));
  EXPECT_FALSE(FuzzyMatchesSingleToken(u"john", u"xyz"));
}

// Tests long query tokens (len > 5) which allow up to 2 edits.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_LongTokens) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"street", u"stteet"));
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"passport", u"passprt"));
  EXPECT_FALSE(FuzzyMatchesSingleToken(u"passport", u"abcdefg"));
}

// Tests prefix fuzzy matching for incomplete tokens typed during user input.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesSingleToken_PrefixMatch) {
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"smith", u"smi"));
  EXPECT_TRUE(FuzzyMatchesSingleToken(u"smith", u"smt"));
}

// Tests multi-token ordered fuzzy matching with typos and word skips.
TEST(AtMemoryStringFilteringUtilTest,
     TokenizeAndFuzzyMatchOrderedTokens_Basic) {
  EXPECT_TRUE(TokenizeAndFuzzyMatchOrderedTokens(u"john smith", u"john smith"));
  EXPECT_TRUE(
      TokenizeAndFuzzyMatchOrderedTokens(u"dr john alex smith", u"jhn smi"));
}

// Tests that out-of-order query tokens fail to match.
TEST(AtMemoryStringFilteringUtilTest,
     TokenizeAndFuzzyMatchOrderedTokens_OutOfOrderFails) {
  EXPECT_FALSE(TokenizeAndFuzzyMatchOrderedTokens(u"john smith", u"smi jhn"));
}

// Tests that queries containing more tokens than the target entry fail to
// match.
TEST(AtMemoryStringFilteringUtilTest,
     TokenizeAndFuzzyMatchOrderedTokens_TooManyQueryTokens) {
  EXPECT_FALSE(
      TokenizeAndFuzzyMatchOrderedTokens(u"john smith", u"john alex smith jr"));
}

// Tests that an empty filter string matches any target string.
TEST(AtMemoryStringFilteringUtilTest,
     TokenizeAndFuzzyMatchOrderedTokens_EmptyFilter) {
  EXPECT_TRUE(TokenizeAndFuzzyMatchOrderedTokens(u"john smith", u""));
}

// Tests that all entities are matched: codes by substring, free text fuzzily.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_SupportedEntities) {
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressZip, u"94043",
                                   RawValueQuery(u"94043")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                   u"mountain view",
                                   RawValueQuery(u"mountain view")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kVehicleVin,
                                   u"1hgcm82633a004352",
                                   RawValueQuery(u"1hgcm82633a004352")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kVehicleMake, u"volvo",
                                   RawValueQuery(u"volvo")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kIban,
                                   u"de91100000000123456789",
                                   RawValueQuery(u"de91100000000123456789")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kCreditCardNumber,
                                   u"4111111111111111",
                                   RawValueQuery(u"4111111111111111")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kCreditCardNameOnCard,
                                   u"john doe", RawValueQuery(u"john doe")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kLoyaltyMembershipId,
                                   u"1234567890", RawValueQuery(u"345")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kLoyaltyMembershipProgram,
                                   u"bahnbonus", RawValueQuery(u"bahnbonuz")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kPassportName,
                                   u"pippi langstrump",
                                   RawValueQuery(u"langstrump")));
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kFlightReservationConfirmationCode,
                           u"abc123", RawValueQuery(u"c12")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kShipmentTrackingNumber,
                                   u"1z 999 aa1 01 2345 6784",
                                   RawValueQuery(u"aa10123")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kOrderMerchantName,
                                   u"example store", RawValueQuery(u"exmple")));
}

// Tests that obfuscated numbers, dates, amounts and nicknames are never
// matched.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_UnsupportedTypes) {
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kPassportNumber,
                                    u"x1234567", RawValueQuery(u"x1234567")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kCreditCardSecurityCode,
                                    u"123", RawValueQuery(u"123")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kPassportExpirationDate,
                                    u"2030 01 01",
                                    RawValueQuery(u"2030 01 01")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kOrderGrandTotal, u"42 00",
                                    RawValueQuery(u"42 00")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kIbanNickname, u"savings",
                                    RawValueQuery(u"savings")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kCreditCardNickname,
                                    u"my card", RawValueQuery(u"my card")));
}

// Tests that codes match if they contain the query anywhere.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_CodeSubstring) {
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kPhone, u"3321436743",
                                   RawValueQuery(u"332143")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kPhone, u"3321436743",
                                   RawValueQuery(u"2143")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kPhone, u"48 123 456 789",
                                   RawValueQuery(u"123")));
}

// Tests that queries need at least 2 characters to match.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_MinimumQueryLength) {
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kAddressZip, u"94043",
                                    RawValueQuery(u"9")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressZip, u"94043",
                                   RawValueQuery(u"94")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                    u"mountain view", RawValueQuery(u"m")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                   u"mountain view", RawValueQuery(u"mo")));
  // Queries made of punctuation only are empty once normalized.
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                    u"mountain view", RawValueQuery(u"")));
}

// Tests the raw-value queries listed in go/at-memory-content-search.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_DesignDocExamples) {
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressStreetAddress,
                                   u"mlynarska 14", RawValueQuery(u"mlynar")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressZip, u"94043",
                                   RawValueQuery(u"94043")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kPhone, u"3321436743",
                                   RawValueQuery(u"332143")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kVehicleModel, u"xc90",
                                   RawValueQuery(u"xc9")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kVehiclePlateNumber,
                                   u"6xyz123", RawValueQuery(u"6xyz")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kIban,
                                   u"pl10 5000 0099 7603 1234 5678 9123",
                                   RawValueQuery(u"pl10")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kIban,
                                   u"pl10 5000 0099 7603 1234 5678 9123",
                                   RawValueQuery(u"pl1050")));
}

// Tests that symbols do not count towards the minimum query length, since they
// are not matched: queries of only symbols, or of a symbol and a single
// character, match nothing.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_Symbols) {
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                    u"mountain view", RawValueQuery(u"++")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kPhone, u"3321436743",
                                    RawValueQuery(u"++")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kAddressStreetAddress,
                                    u"1600 amphitheatre parkway",
                                    RawValueQuery(u"+1")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kPhone, u"+1 650 253",
                                    RawValueQuery(u"+1")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressStreetAddress,
                                   u"1600 amphitheatre parkway",
                                   RawValueQuery(u"+16")));
}

// Tests that whitespace is ignored when matching codes.
TEST(AtMemoryStringFilteringUtilTest,
     MatchesRawValueQuery_CodeIgnoresWhitespace) {
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kPhone, u"332 143 6743",
                                   RawValueQuery(u"332143")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kPhone, u"332 143 6743",
                                   RawValueQuery(u"143 6743")));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kIban, u"pl1050000099",
                                   RawValueQuery(u"pl10 5000")));
}

// Tests that typos are tolerated in free text but not in codes.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_TypoTolerance) {
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                   u"mountain view",
                                   RawValueQuery(u"montain")));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kVehiclePlateNumber,
                                    u"6xyz123", RawValueQuery(u"6xya")));
}

// Tests that all words of a free text query must match in order.
TEST(AtMemoryStringFilteringUtilTest,
     MatchesRawValueQuery_TextRequiresAllWordsInOrder) {
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressStreetAddress,
                                   u"1600 amphitheatre parkway",
                                   RawValueQuery(u"amphitheatre park")));
  EXPECT_FALSE(MatchesRawValueQuery(
      MemoryDataType::kAddressCity, u"tokyo",
      RawValueQuery(u"amount from receipt last week at eat tokyo")));
}

}  // namespace

}  // namespace autofill
