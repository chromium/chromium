// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/common/persisted_trial_token.h"

#include <tuple>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/json/values_util.h"
#include "base/values.h"

namespace origin_trials {

namespace {

auto to_tuple(const PersistedTrialToken& token) {
  return std::tie(token.match_subdomains, token.trial_name, token.token_expiry,
                  token.usage_restriction, token.token_signature);
}

}  // namespace

PersistedTrialToken::PersistedTrialToken(
    bool match_subdomains,
    std::string name,
    base::Time expiry,
    blink::TrialToken::UsageRestriction usage,
    std::string signature,
    base::flat_set<std::string> partitions)
    : match_subdomains(match_subdomains),
      trial_name(std::move(name)),
      token_expiry(expiry),
      usage_restriction(usage),
      token_signature(std::move(signature)),
      partition_sites(std::move(partitions)) {}

PersistedTrialToken::PersistedTrialToken(const blink::TrialToken& parsed_token,
                                         const std::string& partition_site)
    : PersistedTrialToken(parsed_token.match_subdomains(),
                          parsed_token.feature_name(),
                          parsed_token.expiry_time(),
                          parsed_token.usage_restriction(),
                          parsed_token.signature(),
                          base::flat_set<std::string>()) {
  AddToPartition(partition_site);
}

PersistedTrialToken::~PersistedTrialToken() = default;
PersistedTrialToken::PersistedTrialToken(const PersistedTrialToken&) = default;
PersistedTrialToken& PersistedTrialToken::operator=(
    const PersistedTrialToken&) = default;
PersistedTrialToken::PersistedTrialToken(PersistedTrialToken&&) = default;
PersistedTrialToken& PersistedTrialToken::operator=(PersistedTrialToken&&) =
    default;

void PersistedTrialToken::AddToPartition(const std::string& partition_site) {
  DCHECK_NE("", partition_site);
  partition_sites.insert(partition_site);
}

void PersistedTrialToken::RemoveFromPartition(
    const std::string& partition_site) {
  partition_sites.erase(partition_site);
}

bool PersistedTrialToken::InAnyPartition() const {
  return partition_sites.size() > 0;
}

bool PersistedTrialToken::Matches(const blink::TrialToken& token) const {
  return match_subdomains == token.match_subdomains() &&
         trial_name == token.feature_name() &&
         token_expiry == token.expiry_time() &&
         token_signature == token.signature();
}

bool operator<(const PersistedTrialToken& a, const PersistedTrialToken& b) {
  return to_tuple(a) < to_tuple(b);
}

bool operator==(const PersistedTrialToken& a, const PersistedTrialToken& b) {
  return to_tuple(a) == to_tuple(b) && a.partition_sites == b.partition_sites;
}

bool operator!=(const PersistedTrialToken& a, const PersistedTrialToken& b) {
  return !(a == b);
}

std::ostream& operator<<(std::ostream& out, const PersistedTrialToken& token) {
  out << "{";
  out << "match_subdomains: " << (token.match_subdomains ? "true" : "false")
      << ", ";
  out << "trial: " << token.trial_name << ", ";
  out << "expiry: " << base::TimeToValue(token.token_expiry) << ", ";
  out << "usage: " << static_cast<int>(token.usage_restriction) << ", ";
  out << "signature: " << base::Base64Encode(token.token_signature) << ", ";
  out << "partition_sites: [";
  for (const auto& site : token.partition_sites) {
    out << site << " ";
  }
  out << "]";
  out << "}";
  return out;
}

}  // namespace origin_trials
