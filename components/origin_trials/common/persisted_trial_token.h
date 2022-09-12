// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_COMMON_PERSISTED_TRIAL_TOKEN_H_
#define COMPONENTS_ORIGIN_TRIALS_COMMON_PERSISTED_TRIAL_TOKEN_H_

#include <string>

#include "base/time/time.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"

namespace origin_trials {

// Parsed information about a token to be persisted.
struct PersistedTrialToken {
  std::string trial_name;
  base::Time token_expiry;
  blink::TrialToken::UsageRestriction usage_restriction;
  std::string token_signature;

  PersistedTrialToken(std::string name,
                      base::Time expiry,
                      blink::TrialToken::UsageRestriction usage,
                      std::string signature)
      : trial_name(std::move(name)),
        token_expiry(expiry),
        usage_restriction(usage),
        token_signature(std::move(signature)) {}
};

// Comparison operator to let us store PersistedTokens in a flat_set
bool operator<(const PersistedTrialToken& a, const PersistedTrialToken& b);

}  // namespace origin_trials

#endif  // COMPONENTS_ORIGIN_TRIALS_COMMON_PERSISTED_TRIAL_TOKEN_H_
