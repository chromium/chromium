// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/common/persisted_trial_token.h"

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

}  // namespace

}  // namespace origin_trials
