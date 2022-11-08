// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_PROTO_PROTO_UTIL_H_
#define COMPONENTS_ORIGIN_TRIALS_PROTO_PROTO_UTIL_H_

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "components/origin_trials/proto/db_trial_token.pb.h"
#include "url/origin.h"

namespace origin_trials_pb {

uint64_t SerializeTime(const base::Time& time);
base::Time DeserializeTime(uint64_t serialized);
origin_trials_pb::TrialTokenDbEntries ProtoFromTokens(
    const url::Origin& origin,
    const base::flat_set<origin_trials::PersistedTrialToken>& tokens);

}  // namespace origin_trials_pb

#endif  // COMPONENTS_ORIGIN_TRIALS_PROTO_PROTO_UTIL_H_
