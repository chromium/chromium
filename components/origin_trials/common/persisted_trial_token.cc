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

const char kExpiryTimeKey[] = "expiry";
const char kSignatureKey[] = "signature";
const char kTrialNameKey[] = "trial";
const char kUsageKey[] = "usage";

absl::optional<base::Time> TimeFromDict(const base::Value::Dict& dict,
                                        const char* key) {
  const base::Value* time_val = dict.Find(key);
  if (!time_val) {
    return absl::nullopt;
  }
  return base::ValueToTime(time_val);
}

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

// static
absl::optional<PersistedTrialToken> PersistedTrialToken::FromDict(
    const base::Value::Dict& dict) {
  const std::string* name = dict.FindString(kTrialNameKey);
  if (!name) {
    return absl::nullopt;
  }

  absl::optional<base::Time> expiry = TimeFromDict(dict, kExpiryTimeKey);
  if (!expiry) {
    return absl::nullopt;
  }

  absl::optional<int> usage = dict.FindInt(kUsageKey);
  if (!usage) {
    return absl::nullopt;
  }

  const std::string* signature_blob = dict.FindString(kSignatureKey);
  if (!signature_blob) {
    return absl::nullopt;
  }

  std::string signature_string;
  if (!base::Base64Decode(*signature_blob, &signature_string)) {
    return absl::nullopt;
  }

  return absl::make_optional<PersistedTrialToken>(
      *name, *expiry, static_cast<blink::TrialToken::UsageRestriction>(*usage),
      std::move(signature_string));
}

base::Value::Dict PersistedTrialToken::AsDict() const {
  base::Value::Dict token_dict;

  token_dict.Set(kTrialNameKey, trial_name);
  token_dict.Set(kExpiryTimeKey, base::TimeToValue(token_expiry));
  token_dict.Set(kUsageKey, static_cast<int>(usage_restriction));

  // The signature is a binary blob, but the json writer used is not allowed to
  // emit binary blobs, so saving it as base64.
  // Simply saving it as a string will not work, as it may not be valid utf-8.
  std::string signature_blob;
  base::Base64Encode(token_signature, &signature_blob);
  token_dict.Set(kSignatureKey, signature_blob);

  return token_dict;
}

std::ostream& operator<<(std::ostream& out, const PersistedTrialToken& token) {
  return out << token.AsDict();  // Dict already has a streaming operator.
}

}  // namespace origin_trials
