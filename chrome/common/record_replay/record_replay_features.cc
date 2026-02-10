// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/record_replay/record_replay_features.h"

#include "base/feature_list.h"

namespace record_replay::features {

// Enables the record & replay feature in its most basic form.
BASE_FEATURE(kRecordReplayBase, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace record_replay::features
