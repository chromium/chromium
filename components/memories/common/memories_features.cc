// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memories/common/memories_features.h"

#include "build/build_config.h"

namespace memories {

// Enables the Chrome Memories history clustering feature.
const base::Feature kChromeMemories{
    "ChromeMemories",
    base::FEATURE_ENABLED_BY_DEFAULT,
};

}  // namespace memories
