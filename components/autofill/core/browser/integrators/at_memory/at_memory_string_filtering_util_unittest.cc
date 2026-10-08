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
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesOrderedTokens_Basic) {
  EXPECT_TRUE(FuzzyMatchesOrderedTokens(u"john smith", u"john smith"));
  EXPECT_TRUE(FuzzyMatchesOrderedTokens(u"dr john alex smith", u"jhn smi"));
}

// Tests that out-of-order query tokens fail to match.
TEST(AtMemoryStringFilteringUtilTest,
     FuzzyMatchesOrderedTokens_OutOfOrderFails) {
  EXPECT_FALSE(FuzzyMatchesOrderedTokens(u"john smith", u"smi jhn"));
}

// Tests that queries containing more tokens than the target entry fail to
// match.
TEST(AtMemoryStringFilteringUtilTest,
     FuzzyMatchesOrderedTokens_TooManyQueryTokens) {
  EXPECT_FALSE(FuzzyMatchesOrderedTokens(u"john smith", u"john alex smith jr"));
}

// Tests that an empty filter string matches any target string.
TEST(AtMemoryStringFilteringUtilTest, FuzzyMatchesOrderedTokens_EmptyFilter) {
  EXPECT_TRUE(FuzzyMatchesOrderedTokens(u"john smith", u""));
}

// Tests that all entities are matched: codes by substring, free text fuzzily.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_SupportedEntities) {
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kAddressZip, u"94043", u"94043"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                   u"mountain view", u"mountain view"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kVehicleVin,
                                   u"1hgcm82633a004352", u"1hgcm82633a004352"));
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kVehicleMake, u"volvo", u"volvo"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kIban,
                                   u"de91100000000123456789",
                                   u"de91100000000123456789"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kIbanNickname, u"savings",
                                   u"savings"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kCreditCardNumber,
                                   u"4111111111111111", u"4111111111111111"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kCreditCardNameOnCard,
                                   u"john doe", u"john doe"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kLoyaltyMembershipId,
                                   u"1234567890", u"345"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kLoyaltyMembershipProgram,
                                   u"bahnbonus", u"bahnbonuz"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kPassportName,
                                   u"pippi langstrump", u"langstrump"));
  EXPECT_TRUE(MatchesRawValueQuery(
      MemoryDataType::kFlightReservationConfirmationCode, u"abc123", u"c12"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kShipmentTrackingNumber,
                                   u"1z 999 aa1 01 2345 6784", u"aa10123"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kOrderMerchantName,
                                   u"example store", u"exmple"));
}

// Tests that obfuscated numbers, dates and amounts are never matched.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_UnsupportedTypes) {
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kPassportNumber,
                                    u"x1234567", u"x1234567"));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kCreditCardSecurityCode,
                                    u"123", u"123"));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kPassportExpirationDate,
                                    u"2030 01 01", u"2030 01 01"));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kOrderGrandTotal, u"42 00",
                                    u"42 00"));
}

// Tests that codes match if they contain the query anywhere.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_CodeSubstring) {
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kPhone, u"3321436743", u"332143"));
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kPhone, u"3321436743", u"2143"));
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kPhone, u"48 123 456 789", u"123"));
}

// Tests that queries need at least 2 characters to match.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_MinimumQueryLength) {
  EXPECT_FALSE(
      MatchesRawValueQuery(MemoryDataType::kAddressZip, u"94043", u"9"));
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kAddressZip, u"94043", u"94"));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                    u"mountain view", u"m"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                   u"mountain view", u"mo"));
  // Queries made of punctuation only are empty once normalized.
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                    u"mountain view", u""));
}

// Tests the raw-value queries listed in go/at-memory-content-search.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_DesignDocExamples) {
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressStreetAddress,
                                   u"mlynarska 14", u"mlynar"));
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kAddressZip, u"94043", u"94043"));
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kPhone, u"3321436743", u"332143"));
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kVehicleModel, u"xc90", u"xc9"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kVehiclePlateNumber,
                                   u"6xyz123", u"6xyz"));
  EXPECT_TRUE(MatchesRawValueQuery(
      MemoryDataType::kIban, u"pl10 5000 0099 7603 1234 5678 9123", u"pl10"));
  EXPECT_TRUE(MatchesRawValueQuery(
      MemoryDataType::kIban, u"pl10 5000 0099 7603 1234 5678 9123", u"pl1050"));
}

// Tests that whitespace is ignored when matching codes.
TEST(AtMemoryStringFilteringUtilTest,
     MatchesRawValueQuery_CodeIgnoresWhitespace) {
  EXPECT_TRUE(
      MatchesRawValueQuery(MemoryDataType::kPhone, u"332 143 6743", u"332143"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kPhone, u"332 143 6743",
                                   u"143 6743"));
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kIban, u"pl1050000099",
                                   u"pl10 5000"));
}

// Tests that typos are tolerated in free text but not in codes.
TEST(AtMemoryStringFilteringUtilTest, MatchesRawValueQuery_TypoTolerance) {
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressCity,
                                   u"mountain view", u"montain"));
  EXPECT_FALSE(MatchesRawValueQuery(MemoryDataType::kVehiclePlateNumber,
                                    u"6xyz123", u"6xya"));
}

// Tests that all words of a free text query must match in order.
TEST(AtMemoryStringFilteringUtilTest,
     MatchesRawValueQuery_TextRequiresAllWordsInOrder) {
  EXPECT_TRUE(MatchesRawValueQuery(MemoryDataType::kAddressStreetAddress,
                                   u"1600 amphitheatre parkway",
                                   u"amphitheatre park"));
  EXPECT_FALSE(
      MatchesRawValueQuery(MemoryDataType::kAddressCity, u"tokyo",
                           u"amount from receipt last week at eat tokyo"));
}

}  // namespace

}  // namespace autofill
