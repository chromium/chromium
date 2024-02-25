// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_COMMON_PERSISTED_TRIAL_TOKEN_H_
#define COMPONENTS_ORIGIN_TRIALS_COMMON_PERSISTED_TRIAL_TOKEN_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"

namespace origin_trials {

// Parsed information about a token to be persisted.
//
// The token stores partitioning information as a set of strings, but this is
// not part of the tokens sort order with respect to being inserted in sorted
// sets.
struct PersistedTrialToken {
  bool match_subdomains;
  std::string trial_name;
  base::Time token_expiry;
  blink::TrialToken::UsageRestriction usage_restriction;
  std::string token_signature;
  base::flat_set<std::string> partition_sites;

  PersistedTrialToken(bool match_subdomains,
                      std::string name,
                      base::Time expiry,
                      blink::TrialToken::UsageRestriction usage,
                      std::string signature,
                      base::flat_set<std::string> partition_sites);
  PersistedTrialToken(const blink::TrialToken& parsed_token,
                      const std::string& partition_site);
  ~PersistedTrialToken();

  PersistedTrialToken(const PersistedTrialToken&);
  PersistedTrialToken& operator=(const PersistedTrialToken&);

  PersistedTrialToken(PersistedTrialToken&&);
  PersistedTrialToken& operator=(PersistedTrialToken&&);

  // Add the trial token to the partition of the passed |partition_site|.
  void AddToPartition(const std::string& partition_site);

  // Removes the trial token from the partition of the given |partition_site|.
  void RemoveFromPartition(const std::string& partition_site);

  // Returns whether this PersistedTrialToken is currently logically in any
  // top-level partition, either first-party or third-party.
  bool InAnyPartition() const;

  // Return true if this token matches the information in |trial_token|,
  // specifically the origin, match subdomains, trial name, expiry time, and
  // signature attributes.
  bool Matches(const blink::TrialToken& trial_token) const;
};

// Comparison operator to let us store PersistedTokens in a flat_set.
// Does not take partitioning metadata into account.
bool operator<(const PersistedTrialToken& a, const PersistedTrialToken& b);

// Equality operator for testing.
bool operator==(const PersistedTrialToken& a, const PersistedTrialToken& b);

// In-equality operator for testing
bool operator!=(const PersistedTrialToken& a, const PersistedTrialToken& b);

// Stream operator, mainly for GTEST output
std::ostream& operator<<(std::ostream& out, const PersistedTrialToken& token);

}  // namespace origin_trials

#endif  // COMPONENTS_ORIGIN_TRIALS_COMMON_PERSISTED_TRIAL_TOKEN_H_
