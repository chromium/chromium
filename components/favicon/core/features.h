// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FEATURES_H_
#define COMPONENTS_FAVICON_CORE_FEATURES_H_

namespace base {
struct Feature;
}

namespace favicon {

extern const base::Feature kAllowPropagationOfFaviconCacheHits;
extern const base::Feature kEnableHistoryFaviconsGoogleServerQuery;

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FEATURES_H_
