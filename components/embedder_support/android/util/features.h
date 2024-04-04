// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_FEATURES_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_FEATURES_H_

#include "base/feature_list.h"

namespace embedder_support::features {

// Enables optimizations to input stream handling.
BASE_DECLARE_FEATURE(kInputStreamOptimizations);

}  // namespace embedder_support::features

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_FEATURES_H_
