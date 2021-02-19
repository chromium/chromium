// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memories/core/memories_features.h"

#include "build/build_config.h"

namespace memories {

// Enables the Chrome Memories history clustering feature.
const base::Feature kMemories{"Memories", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables debug info; e.g. shows visit metadata on chrome://history entries.
const base::Feature kDebug{"MemoriesDebug", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace memories
