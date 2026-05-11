// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/common/record_replay_features.h"

#include "base/feature_list.h"

namespace record_replay::features {

// Enables the record & replay feature in its most basic form.
BASE_FEATURE(kRecordReplayBase, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kRecordReplayTaskDefinitionSeed,
                   &kRecordReplayBase,
                   "");

}  // namespace record_replay::features
