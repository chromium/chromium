// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/common/persisted_trial_token.h"

#include <sstream>
#include <string>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace origin_trials {

namespace {

TEST(PersistedTrialTokenTest, TestLessThan) {
  base::Time expiry = base::Time::Now();
  base::Time higher_expiry = expiry + base::Hours(1);
  std::string signature = "signature_a";
  std::string higher_signature = "signature_b";

  blink::TrialToken::UsageRestriction restriction_none =
      blink::TrialToken::UsageRestriction::kNone;
  blink::TrialToken::UsageRestriction restriction_subset =
      blink::TrialToken::UsageRestriction::kSubset;

  // Tokens should be sorted by name all else being equal
  EXPECT_LT(PersistedTrialToken("a", expiry, restriction_none, signature),
            PersistedTrialToken("b", expiry, restriction_none, signature));
  // Tokens should be sorted by expiry all else being equal
  EXPECT_LT(
      PersistedTrialToken("a", expiry, restriction_none, signature),
      PersistedTrialToken("a", higher_expiry, restriction_none, signature));
  // Tokens should be sorted by usage restriction all else being equal
  EXPECT_LT(PersistedTrialToken("a", expiry, restriction_none, signature),
            PersistedTrialToken("a", expiry, restriction_subset, signature));
  // Tokens should be sorted by signature all else being equal
  EXPECT_LT(
      PersistedTrialToken("a", expiry, restriction_none, signature),
      PersistedTrialToken("a", expiry, restriction_none, higher_signature));

  // Name should be the primary sorting factor
  EXPECT_LT(
      PersistedTrialToken("a", higher_expiry, restriction_none, signature),
      PersistedTrialToken("b", expiry, restriction_none, signature));
  EXPECT_LT(PersistedTrialToken("a", expiry, restriction_subset, signature),
            PersistedTrialToken("b", expiry, restriction_none, signature));
  EXPECT_LT(
      PersistedTrialToken("a", expiry, restriction_none, higher_signature),
      PersistedTrialToken("b", expiry, restriction_none, signature));

  // Expiry should be the secondary sorting factor
  EXPECT_LT(
      PersistedTrialToken("a", expiry, restriction_subset, signature),
      PersistedTrialToken("a", higher_expiry, restriction_none, signature));
  EXPECT_LT(
      PersistedTrialToken("a", expiry, restriction_none, higher_signature),
      PersistedTrialToken("a", higher_expiry, restriction_none, signature));
  // Subset should be the third sorting factor
  EXPECT_LT(
      PersistedTrialToken("a", expiry, restriction_none, higher_signature),
      PersistedTrialToken("a", expiry, restriction_subset, signature));
}

TEST(PersistedTrialTokenTest, TestEquals) {
  base::Time expiry = base::Time::Now();
  base::Time higher_expiry = expiry + base::Hours(1);
  std::string signature = "signature_a";
  std::string higher_signature = "signature_b";
  blink::TrialToken::UsageRestriction restriction_none =
      blink::TrialToken::UsageRestriction::kNone;
  blink::TrialToken::UsageRestriction restriction_subset =
      blink::TrialToken::UsageRestriction::kSubset;
  // Two tokens with equal objects should be equal
  EXPECT_EQ(PersistedTrialToken("a", expiry, restriction_none, signature),
            PersistedTrialToken("a", expiry, restriction_none, signature));

  // Tokens should not be equal if their fields differ
  EXPECT_FALSE(PersistedTrialToken("a", expiry, restriction_none, signature) ==
               PersistedTrialToken("b", expiry, restriction_none, signature));

  EXPECT_FALSE(
      PersistedTrialToken("a", expiry, restriction_none, signature) ==
      PersistedTrialToken("a", higher_expiry, restriction_none, signature));

  EXPECT_FALSE(PersistedTrialToken("a", expiry, restriction_none, signature) ==
               PersistedTrialToken("a", expiry, restriction_subset, signature));

  EXPECT_FALSE(
      PersistedTrialToken("a", expiry, restriction_none, signature) ==
      PersistedTrialToken("a", expiry, restriction_none, higher_signature));
}

TEST(PersistedTrialTokenTest, TokenToDictRoundTrip) {
  PersistedTrialToken token("a", base::Time::Now(),
                            blink::TrialToken::UsageRestriction::kNone,
                            "signature");
  EXPECT_EQ(token, PersistedTrialToken::FromDict(token.AsDict()));

  token = PersistedTrialToken("a", base::Time::Now(),
                              blink::TrialToken::UsageRestriction::kSubset,
                              "signature");
  EXPECT_EQ(token, PersistedTrialToken::FromDict(token.AsDict()));
}

TEST(PersistedTrialTokenTest, InvalidDictParsing) {
  // Do not expect an empty dict to parse
  EXPECT_FALSE(PersistedTrialToken::FromDict(base::Value::Dict()));

  // Test that parsing fails if we remove one of the keys in a valid dict
  PersistedTrialToken token("a", base::Time::Now(),
                            blink::TrialToken::UsageRestriction::kNone,
                            "token_signature");
  const base::Value::Dict token_dict = token.AsDict();
  for (const auto entry : token_dict) {
    base::Value::Dict faulty_dict = token_dict.Clone();
    faulty_dict.Remove(entry.first);
    EXPECT_FALSE(PersistedTrialToken::FromDict(faulty_dict))
        << "Did not expect dict to parse with key " << entry.first
        << " missing.";
  }

  // Test that parsing fails if the signature cannot be base-64 decoded
  base::Value::Dict faulty_dict = token_dict.Clone();
  faulty_dict.Set("signature", "This is not valid Base64 data!");
  EXPECT_FALSE(PersistedTrialToken::FromDict(faulty_dict));
}

TEST(PersistedTrialTokenTest, StreamOperatorTest) {
  // Ensure that streaming the token produces the same result as converting
  // to a dict and streaming that.
  PersistedTrialToken token("a", base::Time::Now(),
                            blink::TrialToken::UsageRestriction::kSubset,
                            "signature");
  std::ostringstream token_stream;
  token_stream << token;
  std::string token_str = token_stream.str();

  std::ostringstream dict_stream;
  dict_stream << token.AsDict();
  std::string dict_str = dict_stream.str();

  EXPECT_EQ(token_str, dict_str);
}

}  // namespace

}  // namespace origin_trials
