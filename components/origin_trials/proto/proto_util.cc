// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/proto/proto_util.h"

namespace origin_trials_pb {

// Recommended serialization as per |base::Time|
uint64_t SerializeTime(const base::Time& time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// Recommended deserialization as per |base::Time|
base::Time DeserializeTime(uint64_t serialized) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(serialized));
}

origin_trials_pb::TrialTokenDbEntries ProtoFromTokens(
    const url::Origin& origin,
    const base::flat_set<origin_trials::PersistedTrialToken>& tokens) {
  origin_trials_pb::TrialTokenDbEntries entries;
  origin_trials_pb::OriginMessage* origin_message = entries.mutable_origin();
  origin_message->set_scheme(origin.scheme());
  origin_message->set_host(origin.host());
  origin_message->set_port(origin.port());

  for (const auto& token : tokens) {
    origin_trials_pb::TrialTokenDbEntry* proto = entries.add_tokens();
    proto->set_trial_name(token.trial_name);
    proto->set_match_subdomains(token.match_subdomains);
    proto->set_token_expiry(
        origin_trials_pb::SerializeTime(token.token_expiry));
    proto->set_token_signature(token.token_signature);
    proto->set_usage_restriction(
        static_cast<uint32_t>(token.usage_restriction));
    for (const auto& site : token.partition_sites) {
      proto->add_partition_sites(site);
    }
  }
  return entries;
}

}  // namespace origin_trials_pb
