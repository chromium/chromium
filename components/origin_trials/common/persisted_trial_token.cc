// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/common/persisted_trial_token.h"

#include <tuple>

#include "base/base64.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace origin_trials {

namespace {

auto to_tuple(const PersistedTrialToken& token) {
  return std::tie(token.trial_name, token.token_expiry, token.usage_restriction,
                  token.token_signature);
}

}  // namespace

bool operator<(const PersistedTrialToken& a, const PersistedTrialToken& b) {
  return to_tuple(a) < to_tuple(b);
}

bool operator==(const PersistedTrialToken& a, const PersistedTrialToken& b) {
  return to_tuple(a) == to_tuple(b);
}

std::ostream& operator<<(std::ostream& out, const PersistedTrialToken& token) {
  out << "{";
  out << "trial: " << token.trial_name << ", ";
  out << "expiry: " << base::TimeToValue(token.token_expiry) << ", ";
  out << "usage: " << static_cast<int>(token.usage_restriction) << ", ";
  std::string signature_blob;
  base::Base64Encode(token.token_signature, &signature_blob);
  out << "signature: " << signature_blob;
  out << "}";
  return out;
}

}  // namespace origin_trials
