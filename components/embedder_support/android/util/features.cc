// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/features.h"

#include "base/feature_list.h"

namespace embedder_support::features {

BASE_FEATURE(kInputStreamOptimizations,
             "InputStreamOptimizations",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace embedder_support::features
