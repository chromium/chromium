// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/common/persisted_trial_token.h"

namespace origin_trials {

bool operator<(const PersistedTrialToken& a, const PersistedTrialToken& b) {
  if (a.trial_name != b.trial_name) {
    return a.trial_name < b.trial_name;
  }
  if (a.token_expiry != b.token_expiry) {
    return a.token_expiry < b.token_expiry;
  }
  if (a.usage_restriction != b.usage_restriction) {
    return a.usage_restriction < b.usage_restriction;
  }
  return a.token_signature < b.token_signature;
}

}  // namespace origin_trials
