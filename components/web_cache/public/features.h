// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_CACHE_PUBLIC_FEATURES_H_
#define COMPONENTS_WEB_CACHE_PUBLIC_FEATURES_H_

#include "base/feature_list.h"

namespace web_cache {

// Disable the logic that try to coordinate the in-memory cache resource usage
// of all renderers and simply trim the caches on memory pressure. Renderers
// get a memory pressure signal a few minutes after they've been backgrounded.
BASE_DECLARE_FEATURE(kTrimWebCacheOnMemoryPressureOnly);

}  // namespace web_cache

#endif  // COMPONENTS_WEB_CACHE_PUBLIC_FEATURES_H_
