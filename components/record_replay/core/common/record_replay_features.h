// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_COMMON_RECORD_REPLAY_FEATURES_H_
#define COMPONENTS_RECORD_REPLAY_CORE_COMMON_RECORD_REPLAY_FEATURES_H_

#include "base/feature_list.h"

namespace record_replay::features {

BASE_DECLARE_FEATURE(kRecordReplayBase);

BASE_DECLARE_FEATURE_PARAM(std::string, kRecordReplayTaskDefinitionSeed);

}  // namespace record_replay::features

#endif  // COMPONENTS_RECORD_REPLAY_CORE_COMMON_RECORD_REPLAY_FEATURES_H_
