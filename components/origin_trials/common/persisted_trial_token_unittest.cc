// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/common/persisted_trial_token.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "url/origin.h"

namespace origin_trials {

namespace {

const char kTrialTopLevelSite[] = "example.com";

TEST(PersistedTrialTokenTest, ConstructFromBlinkToken) {
  url::Origin origin =
      url::Origin::CreateFromNormalizedTuple("https", "example.com", 443);
  std::string trial_name = "FrobulatePersistent";
  base::Time expiry = base::Time::Now();
  std::string signature = "signature";

  std::unique_ptr<blink::TrialToken> blink_token =
      blink::TrialToken::CreateTrialTokenForTesting(
          origin, /*match_subdomains=*/false, trial_name, expiry,
          /*is_third_party=*/false,
          blink::TrialToken::UsageRestriction::kSubset, signature);
  PersistedTrialToken token(*blink_token, kTrialTopLevelSite);

  EXPECT_EQ(trial_name, token.trial_name);
  EXPECT_EQ(expiry, token.token_expiry);
  EXPECT_EQ(signature, token.token_signature);
  EXPECT_EQ(blink::TrialToken::UsageRestriction::kSubset,
            token.usage_restriction);
}

TEST(PersistedTrialTokenTest, Partitioning) {
  std::string trial_name = "FrobulatePersistent";
  base::Time expiry = base::Time::Now();
  std::string signature = "signature";

  base::flat_set<std::string> partition_sites = {kTrialTopLevelSite};
  PersistedTrialToken token(/*match_subdomains=*/false, trial_name, expiry,
                            blink::TrialToken::UsageRestriction::kNone,
                            signature, partition_sites);

  EXPECT_TRUE(token.InAnyPartition())
      << "Was constructed with one partition set";

  token.RemoveFromPartition(kTrialTopLevelSite);
  EXPECT_FALSE(token.InAnyPartition())
      << "Should not be in any partition after removal";

  token.AddToPartition(kTrialTopLevelSite);
  EXPECT_TRUE(token.InAnyPartition()) << "Added to a partition";
}

TEST(PersistedTrialTokenTest, TestLessThan) {
  base::Time expiry = base::Time::Now();
  base::Time higher_expiry = expiry + base::Hours(1);
  std::string signature = "signature_a";
  std::string higher_signature = "signature_b";
  base::flat_set<std::string> partition_sites = {kTrialTopLevelSite};

  blink::TrialToken::UsageRestriction restriction_none =
      blink::TrialToken::UsageRestriction::kNone;
  blink::TrialToken::UsageRestriction restriction_subset =
      blink::TrialToken::UsageRestriction::kSubset;

  // Tokens should be sorted by subdomain matching all else being equal
  EXPECT_LT(PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature, partition_sites),
            PersistedTrialToken(/*match_subdomains=*/true, "a", expiry,
                                restriction_none, signature, partition_sites));
  // Tokens should be sorted by name all else being equal
  EXPECT_LT(PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature, partition_sites),
            PersistedTrialToken(/*match_subdomains=*/false, "b", expiry,
                                restriction_none, signature, partition_sites));
  // Tokens should be sorted by expiry all else being equal
  EXPECT_LT(PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature, partition_sites),
            PersistedTrialToken(/*match_subdomains=*/false, "a", higher_expiry,
                                restriction_none, signature, partition_sites));
  // Tokens should be sorted by usage restriction all else being equal
  EXPECT_LT(
      PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                          restriction_none, signature, partition_sites),
      PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                          restriction_subset, signature, partition_sites));
  // Tokens should be sorted by signature all else being equal
  EXPECT_LT(
      PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                          restriction_none, signature, partition_sites),
      PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                          restriction_none, higher_signature, partition_sites));

  // Partition set is not part of sort order / token identity (for sets)
  EXPECT_FALSE(PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                   restriction_none, signature,
                                   base::flat_set<std::string>()) <
               PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                   restriction_none, signature,
                                   partition_sites));

  EXPECT_FALSE(PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                   restriction_none, signature,
                                   partition_sites) <
               PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                   restriction_none, signature,
                                   base::flat_set<std::string>()));
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
  base::flat_set<std::string> partition_sites = {kTrialTopLevelSite};
  // Two tokens with equal objects should be equal
  EXPECT_EQ(PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature, partition_sites),
            PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature, partition_sites));

  // Tokens should not be equal if their fields differ
  EXPECT_NE(PersistedTrialToken(/*match_subdomains=*/true, "a", expiry,
                                restriction_none, signature, partition_sites),
            PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature, partition_sites));

  EXPECT_NE(PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature, partition_sites),
            PersistedTrialToken(/*match_subdomains=*/false, "b", expiry,
                                restriction_none, signature, partition_sites));

  EXPECT_NE(PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature, partition_sites),
            PersistedTrialToken(/*match_subdomains=*/false, "a", higher_expiry,
                                restriction_none, signature, partition_sites));

  EXPECT_NE(
      PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                          restriction_none, signature, partition_sites),
      PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                          restriction_subset, signature, partition_sites));

  EXPECT_NE(
      PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                          restriction_none, signature, partition_sites),
      PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                          restriction_none, higher_signature, partition_sites));

  EXPECT_NE(PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature, partition_sites),
            PersistedTrialToken(/*match_subdomains=*/false, "a", expiry,
                                restriction_none, signature,
                                base::flat_set<std::string>()));
}

TEST(PersistedTrialTokenTest, StreamOperatorTest) {
  // Ensure that streaming the token produces non-empty output that contains the
  // name of the token.
  base::flat_set<std::string> partition_sites = {kTrialTopLevelSite};
  std::string token_name = "TokenNameInExpectedOutput";
  PersistedTrialToken token(/*match_subdomains=*/false, token_name,
                            base::Time::Now(),
                            blink::TrialToken::UsageRestriction::kSubset,
                            "signature", partition_sites);
  std::string token_str = base::ToString(token);
  EXPECT_NE("", token_str);
  EXPECT_NE(std::string::npos, token_str.find(token_name));
}

}  // namespace

}  // namespace origin_trials
